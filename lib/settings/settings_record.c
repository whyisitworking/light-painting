#include "settings.h"

#include <string.h>

static const uint8_t magic[4] = {'L', 'P', 'S', 'T'};

constexpr size_t HEADER_SIZE = 12;
constexpr size_t CRC_SIZE = 4;

// The most values a record has room for
constexpr size_t MAX_VALUES =
    (SETTINGS_RECORD_SIZE - HEADER_SIZE - CRC_SIZE) / sizeof(int16_t);

static_assert(SETTINGS_ID_COUNT <= MAX_VALUES,
              "the settings must fit a record");

// CRC-32 as in zlib and Ethernet, bit by bit: a record is short
static uint32_t crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }

    return ~crc;
}

static void put_u16(uint8_t *at, uint16_t value) {
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *at, uint32_t value) {
    put_u16(at, (uint16_t)value);
    put_u16(at + 2, (uint16_t)(value >> 16));
}

static uint16_t get_u16(const uint8_t *at) {
    return (uint16_t)(at[0] | at[1] << 8);
}

static uint32_t get_u32(const uint8_t *at) {
    return get_u16(at) | (uint32_t)get_u16(at + 2) << 16;
}

void settings_encode(const settings_t *this, uint32_t sequence,
                     uint8_t record[SETTINGS_RECORD_SIZE]) {
    size_t size = HEADER_SIZE + SETTINGS_ID_COUNT * sizeof(int16_t);

    memset(record, 0xFF, SETTINGS_RECORD_SIZE);
    memcpy(record, magic, sizeof(magic));
    put_u16(record + 4, SETTINGS_VERSION);
    put_u16(record + 6, SETTINGS_ID_COUNT);
    put_u32(record + 8, sequence);

    for (size_t id = 0; id < SETTINGS_ID_COUNT; id++)
        put_u16(record + HEADER_SIZE + id * sizeof(int16_t),
                (uint16_t)this->values[id]);

    put_u32(record + size, crc32(record, size));
}

bool settings_decode(settings_t *this, uint32_t *sequence,
                     const uint8_t record[SETTINGS_RECORD_SIZE]) {
    size_t count, size;

    if (memcmp(record, magic, sizeof(magic)) != 0 ||
        get_u16(record + 4) != SETTINGS_VERSION)
        return false;

    count = get_u16(record + 6);
    if (count == 0 || count > MAX_VALUES)
        return false;

    size = HEADER_SIZE + count * sizeof(int16_t);
    if (get_u32(record + size) != crc32(record, size))
        return false;

    settings_reset(this);
    for (size_t id = 0; id < count && id < SETTINGS_ID_COUNT; id++)
        this->values[id] =
            (int16_t)get_u16(record + HEADER_SIZE + id * sizeof(int16_t));
    settings_clamp(this);

    *sequence = get_u32(record + 8);

    return true;
}
