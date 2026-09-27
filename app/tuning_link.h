#ifndef APP_TUNING_LINK_H
#define APP_TUNING_LINK_H

/**
 * Hands the tuning from the menu on core 1 to the visualizer on core 0,
 * through a lock-free swapchain: neither core ever waits for the other.
 * Core 0 takes the newest tuning, if there is one, once per hop. Tunings
 * published faster than that are replaced, only the newest counts.
 */

#include "visualizer.h"

// Before either core uses it. False if memory runs out
[[nodiscard]] bool tuning_link_init(void);

// Core 1: publishes a copy of tuning
void tuning_link_publish(const visualizer_tuning_t *tuning);

/**
 * Core 0: the tuning published last, or nullptr if nothing was published
 * since the previous call. Valid until the next call
 */
const visualizer_tuning_t *tuning_link_take(void);

#endif
