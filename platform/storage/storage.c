#include "storage.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/platform.h"

// Where the firmware ends in flash, from the linker script
extern char __flash_binary_end;

// The region, from the start of the flash, and its size
static uint32_t region_offset;
static size_t region_size;
static bool is_init = false;

typedef struct {
    uint32_t offset;
    const uint8_t *data;
    size_t size;
} write_t;

// Run by flash_safe_execute(), with the flash safe to change
static void erase(void *param) {
    const write_t *write = param;

    flash_range_erase(write->offset, FLASH_SECTOR_SIZE);
}

static void program(void *param) {
    const write_t *write = param;

    flash_range_program(write->offset, write->data, write->size);
}

bool storage_init(size_t size) {
    uintptr_t binary_end = (uintptr_t)&__flash_binary_end - XIP_BASE;

    if (is_init || size == 0 || size % FLASH_SECTOR_SIZE != 0 ||
        size > PICO_FLASH_SIZE_BYTES ||
        PICO_FLASH_SIZE_BYTES - size < binary_end)
        return false;

    region_offset = PICO_FLASH_SIZE_BYTES - size;
    region_size = size;
    is_init = true;

    return true;
}

const uint8_t *storage_data(void) {
    return is_init ? (const uint8_t *)(XIP_BASE + region_offset) : nullptr;
}

// Only core 1 may stop reading the flash, see storage.h
static bool can_write(size_t offset, size_t size) {
    return is_init && get_core_num() == 1 && offset <= region_size &&
           size <= region_size - offset;
}

bool storage_erase(size_t offset) {
    if (!can_write(offset, FLASH_SECTOR_SIZE) ||
        offset % FLASH_SECTOR_SIZE != 0)
        return false;

    return flash_safe_execute(erase,
                              &(write_t){.offset = region_offset + offset},
                              UINT32_MAX) == PICO_OK;
}

// Whether data lies in the flash, as mapped for reading
static bool is_in_flash(const uint8_t *data, size_t size) {
    uintptr_t start = (uintptr_t)data;

    return start < XIP_BASE + PICO_FLASH_SIZE_BYTES &&
           start + size > XIP_BASE;
}

bool storage_program(size_t offset, const uint8_t *data, size_t size) {
    if (!can_write(offset, size) || offset % FLASH_PAGE_SIZE != 0 ||
        size % FLASH_PAGE_SIZE != 0 || is_in_flash(data, size))
        return false;

    return flash_safe_execute(program,
                              &(write_t){
                                  .offset = region_offset + offset,
                                  .data = data,
                                  .size = size,
                              },
                              UINT32_MAX) == PICO_OK;
}
