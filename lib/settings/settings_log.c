#include "settings.h"

constexpr size_t SLOTS_PER_BLOCK =
    SETTINGS_LOG_BLOCK_SIZE / SETTINGS_RECORD_SIZE;
constexpr size_t SLOT_COUNT = SLOTS_PER_BLOCK * SETTINGS_LOG_BLOCK_COUNT;

static_assert(SETTINGS_LOG_BLOCK_SIZE % SETTINGS_RECORD_SIZE == 0,
              "records must tile a block");

static bool is_erased(const uint8_t *bytes, size_t size) {
    for (size_t i = 0; i < size; i++)
        if (bytes[i] != 0xFF)
            return false;
    return true;
}

static const uint8_t *slot_at(const uint8_t *log, size_t slot) {
    return log + slot * SETTINGS_RECORD_SIZE;
}

void settings_log_scan(settings_log_t *this, settings_t *settings,
                       const uint8_t *log) {
    size_t newest = 0, block, first;

    *this = (settings_log_t){.found = false};
    settings_reset(settings);

    for (size_t slot = 0; slot < SLOT_COUNT; slot++) {
        settings_t candidate;
        uint32_t sequence;

        if (!settings_decode(&candidate, &sequence, slot_at(log, slot)))
            continue;

        if (!this->found || sequence > this->sequence) {
            this->found = true;
            this->sequence = sequence;
            newest = slot;
            *settings = candidate;
        }
    }

    // The first erased slot after the newest record in its block, or else
    // the start of the other block
    block = this->found ? newest / SLOTS_PER_BLOCK : 0;
    first = this->found ? newest + 1 : 0;

    for (size_t slot = first; slot < (block + 1) * SLOTS_PER_BLOCK; slot++)
        if (is_erased(slot_at(log, slot), SETTINGS_RECORD_SIZE)) {
            this->next_offset = slot * SETTINGS_RECORD_SIZE;
            this->erase_first = false;
            return;
        }

    // Without a record, block 0 holds nothing worth keeping either
    if (this->found)
        block = (block + 1) % SETTINGS_LOG_BLOCK_COUNT;

    this->next_offset = block * SETTINGS_LOG_BLOCK_SIZE;
    this->erase_first =
        !is_erased(log + this->next_offset, SETTINGS_LOG_BLOCK_SIZE);
}

uint32_t settings_log_next_sequence(const settings_log_t *this) {
    return this->found ? this->sequence + 1 : 1;
}
