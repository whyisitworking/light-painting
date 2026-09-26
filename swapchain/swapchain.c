#include "swapchain.h"

#define PRODUCER_INDEX 0
#define SHARED_INDEX 1
#define CONSUMER_INDEX 2

static inline void swap_elements(void *arr[], size_t first, size_t second) {
    void *temp = arr[first];
    arr[first] = arr[second];
    arr[second] = temp;
}

bool swapchain_init(swapchain_t *this, size_t buffer_size) {
    void *alloc = malloc(DEFAULT_BUFFER_COUNT * buffer_size);
    if (alloc == NULL)
        return false;

    for (size_t i = 0; i < DEFAULT_BUFFER_COUNT; i++)
        this->buffer_chain[i] = (void *)((size_t)alloc + (i * buffer_size));

    this->mem = alloc;

    return true;
}

void *swapchain_producer_buffer(swapchain_t *this) {
    return this->buffer_chain[PRODUCER_INDEX];
}

void swapchain_producer_swap(swapchain_t *this) {
    swap_elements(this->buffer_chain, SHARED_INDEX, PRODUCER_INDEX);
}

const void *swapchain_consumer_buffer(swapchain_t *this) {
    return this->buffer_chain[CONSUMER_INDEX];
}

void swapchain_consumer_swap(swapchain_t *this) {
    swap_elements(this->buffer_chain, SHARED_INDEX, CONSUMER_INDEX);
}

void swapchain_deinit(swapchain_t *this) { free(this->mem); }
