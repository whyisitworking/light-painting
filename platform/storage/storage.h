#ifndef STORAGE_H
#define STORAGE_H

/**
 * A region at the very end of the flash, for data kept across power cycles:
 * read straight from memory, erased by sector and programmed by page.
 *
 * Erasing (45 to 400 ms per sector) and programming take the flash away
 * from everything that reads it. The firmware runs from RAM (copy_to_ram)
 * and core 0 never reads the flash: only core 1, the one writing, stops,
 * with its interrupts masked meanwhile. So writes are refused on core 0
 */

#include <stddef.h>
#include <stdint.h>

/**
 * size bytes at the end of the flash, a whole number of sectors. False if
 * already initialized, or if the region is not whole sectors or would reach
 * into the firmware
 */
[[nodiscard]] bool storage_init(size_t size);

// The region's contents, as they are in the flash now
const uint8_t *storage_data(void);

// Erases the sector at offset in the region. False on core 0 or out of it
[[nodiscard]] bool storage_erase(size_t offset);

/**
 * Programs size bytes, a whole number of pages, at a page offset in the
 * region. Bits can only be cleared: the pages must have been erased. False
 * on core 0, or out of the region
 */
[[nodiscard]] bool storage_program(size_t offset, const uint8_t *data,
                                   size_t size);

#endif
