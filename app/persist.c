#include "persist.h"

#include "storage.h"

#include <hardware/flash.h>
#include <string.h>

// One erase clears one block of the log, and records are whole pages
static_assert(SETTINGS_LOG_BLOCK_SIZE == FLASH_SECTOR_SIZE,
              "a log block must be one flash sector");
static_assert(SETTINGS_RECORD_SIZE % FLASH_PAGE_SIZE == 0,
              "a record must be whole flash pages");

bool persist_init(void) { return storage_init(SETTINGS_LOG_SIZE); }

void persist_load(settings_t *settings) {
    settings_log_t log;

    if (storage_data() == nullptr) {
        settings_reset(settings);
        return;
    }

    settings_log_scan(&log, settings, storage_data());
}

bool persist_save(const settings_t *settings) {
    static uint8_t record[SETTINGS_RECORD_SIZE];
    settings_log_t log;
    settings_t current, written;
    uint32_t sequence;

    if (storage_data() == nullptr)
        return false;

    settings_log_scan(&log, &current, storage_data());

    if (log.erase_first && !storage_erase(log.next_offset))
        return false;

    settings_encode(settings, settings_log_next_sequence(&log), record);
    if (!storage_program(log.next_offset, record, sizeof(record)))
        return false;

    // What the flash holds now
    return settings_decode(&written, &sequence,
                           storage_data() + log.next_offset) &&
           memcmp(&written, settings, sizeof(written)) == 0;
}
