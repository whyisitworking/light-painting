#include "swapchain.h"

// The role of each slot of buffer_chain
#define PRODUCER_INDEX 0
#define SHARED_INDEX 1
#define CONSUMER_INDEX 2

static inline void swap_elements(void *volatile arr[], size_t first,
                                 size_t second) {
    void *temp = arr[first];
    arr[first] = arr[second];
    arr[second] = temp;
}

bool swapchain_init(swapchain_t *this, size_t buffer_size) {
    char *alloc = (char *)malloc(SWAPCHAIN_BUFFER_COUNT * buffer_size);
    if (alloc == NULL)
        return false;

    for (size_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
        this->buffer_chain[i] = alloc + i * buffer_size;

    this->mem = alloc;
    this->fresh = false;
    this->dropped = 0;

    return true;
}

void *swapchain_producer_buffer(swapchain_t *this) {
    return this->buffer_chain[PRODUCER_INDEX];
}

void swapchain_producer_swap(swapchain_t *this) {
    swap_elements(this->buffer_chain, SHARED_INDEX, PRODUCER_INDEX);

    // The consumer never saw the buffer we just took back
    if (this->fresh)
        this->dropped++;

    this->fresh = true;
}

const void *swapchain_consumer_buffer(swapchain_t *this) {
    return this->buffer_chain[CONSUMER_INDEX];
}

bool swapchain_consumer_swap(swapchain_t *this) {
    // Swapping now would hand back our own previous, older, buffer
    if (!this->fresh)
        return false;

    swap_elements(this->buffer_chain, SHARED_INDEX, CONSUMER_INDEX);
    this->fresh = false;

    return true;
}

void swapchain_deinit(swapchain_t *this) {
    free(this->mem);

    // No buffer pointer outlives the memory
    *this = (swapchain_t){0};
}
