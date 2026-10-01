#include "tuning_link.h"

#include "swapchain.h"

#include <string.h>

static swapchain_t link;

bool tuning_link_init(void) {
    return swapchain_init(&link, sizeof(visualizer_tuning_t));
}

void tuning_link_publish(const visualizer_tuning_t *tuning) {
    // The producer buffer is core 1's alone until the swap publishes it
    memcpy(swapchain_producer_buffer(&link), tuning, sizeof(*tuning));
    swapchain_producer_swap(&link);
}

const visualizer_tuning_t *tuning_link_take(void) {
    if (!swapchain_consumer_swap(&link))
        return nullptr;

    return (const visualizer_tuning_t *)swapchain_consumer_buffer(&link);
}
