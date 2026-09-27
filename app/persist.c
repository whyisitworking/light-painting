#include "persist.h"

#include "storage.h"

#include <string.h>

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
