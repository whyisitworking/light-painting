#include "swapchain.h"

#include <stdint.h>

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
    return swapchain_init_aligned(this, buffer_size, 1);
}

bool swapchain_init_aligned(swapchain_t *this, size_t buffer_size,
                            size_t alignment) {
    size_t stride;
    uintptr_t first;
    void *alloc;

    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        return false;

    // Each buffer starts on an alignment boundary
    stride = (buffer_size + alignment - 1) & ~(alignment - 1);

    // Room to move the first buffer up to the next boundary
    alloc = malloc(DEFAULT_BUFFER_COUNT * stride + alignment - 1);
    if (alloc == NULL)
        return false;

    first = ((uintptr_t)alloc + alignment - 1) & ~(uintptr_t)(alignment - 1);

    for (size_t i = 0; i < DEFAULT_BUFFER_COUNT; i++)
        this->buffer_chain[i] = (void *)(first + i * stride);

    // What malloc returned, to free
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

void *swapchain_producer_exchange(swapchain_t *this, void *filled) {
    // Publish filled through the producer slot. The buffer that was there
    // stays with the producer, still in flight
    this->buffer_chain[PRODUCER_INDEX] = filled;
    swapchain_producer_swap(this);

    // The previously shared buffer, now free
    return this->buffer_chain[PRODUCER_INDEX];
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

void swapchain_deinit(swapchain_t *this) { free(this->mem); }
