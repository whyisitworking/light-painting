#include "swapchain.h"

// The role of each slot of the buffers array
constexpr size_t PRODUCER_INDEX = 0;
constexpr size_t SHARED_INDEX = 1;
constexpr size_t CONSUMER_INDEX = 2;

static inline void swap_slots(void *volatile slots[], size_t first,
                              size_t second) {
    void *temp = slots[first];
    slots[first] = slots[second];
    slots[second] = temp;
}

bool swapchain_init(swapchain_t *this, size_t buffer_size) {
    char *memory = (char *)malloc(SWAPCHAIN_BUFFER_COUNT * buffer_size);
    if (memory == NULL)
        return false;

    for (size_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
        this->buffers[i] = memory + i * buffer_size;

    this->memory = memory;
    this->fresh = false;
    this->dropped = 0;

    return true;
}

void *swapchain_producer_buffer(swapchain_t *this) {
    return this->buffers[PRODUCER_INDEX];
}

void swapchain_producer_swap(swapchain_t *this) {
    swap_slots(this->buffers, SHARED_INDEX, PRODUCER_INDEX);

    // The consumer never saw the buffer we just took back
    if (this->fresh)
        this->dropped++;

    this->fresh = true;
}

const void *swapchain_consumer_buffer(swapchain_t *this) {
    return this->buffers[CONSUMER_INDEX];
}

bool swapchain_consumer_swap(swapchain_t *this) {
    // Swapping now would hand back our own previous, older, buffer
    if (!this->fresh)
        return false;

    swap_slots(this->buffers, SHARED_INDEX, CONSUMER_INDEX);
    this->fresh = false;

    return true;
}

void swapchain_deinit(swapchain_t *this) {
    free(this->memory);

    // No buffer pointer outlives the memory
    *this = (swapchain_t){0};
}
