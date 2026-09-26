#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include <stdbool.h>
#include <stdlib.h>

#define DEFAULT_BUFFER_COUNT 3
#define DEFAULT_RING_SIZE 2

typedef struct {
    // This amazing quote by Herb Sutter guarantees correct alignment for
    // arbitrary data.
    //      "Alignment. Any memory Alignment. Any memory that's allocated
    //      dynamically via new or malloc is guaranteed to be properly aligned
    //      for objects of **any type**, but buffers that are not allocated
    //      dynamically have no such guarantee."
    void *mem;
    void *buffer_chain[DEFAULT_BUFFER_COUNT];
} swapchain_t;

/**
 * Instantiates a swap-
 */
bool swapchain_init(swapchain_t *this, size_t buffer_size);

/**
 * Producer side
 */
void *swapchain_producer_buffer(swapchain_t *this);
void swapchain_producer_swap(swapchain_t *this);

/**
 * Consumer side
 */
const void *swapchain_consumer_buffer(swapchain_t *this);
void swapchain_consumer_swap(swapchain_t *this);

void swapchain_deinit(swapchain_t *this);

#endif