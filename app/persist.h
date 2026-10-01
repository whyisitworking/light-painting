#ifndef APP_PERSIST_H
#define APP_PERSIST_H

/**
 * The settings kept in flash: the settings log (lib/settings) in the storage
 * region at the end of the flash. Loaded by core 0 at boot, before core 1
 * runs, and saved by core 1, the only one that may write the flash.
 */

#include "settings.h"

// Once, before the others. False if the storage region cannot be set up
[[nodiscard]] bool persist_init(void);

// The saved settings, or the defaults if there are none
void persist_load(settings_t *settings);

/**
 * Core 1: saves the settings, erasing a block first when the log needs it,
 * and reads them back. False if the flash refused or they did not read back
 */
[[nodiscard]] bool persist_save(const settings_t *settings);

#endif
