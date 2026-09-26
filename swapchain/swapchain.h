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
    // Swapped from interrupt handlers, hence volatile
    void *volatile buffer_chain[DEFAULT_BUFFER_COUNT];
    // Whether the shared buffer holds data the consumer has not taken yet
    volatile bool fresh;
    // Number of fresh buffers replaced before the consumer took them
    volatile size_t dropped;
} swapchain_t;

/**
 * Instantiates a swap-
 */
bool swapchain_init(swapchain_t *this, size_t buffer_size);

/**
 * Same, with every buffer starting on a multiple of alignment (a power of
 * two), e.g. for DMA ring buffers that must be aligned to their size.
 */
bool swapchain_init_aligned(swapchain_t *this, size_t buffer_size,
                            size_t alignment);

/**
 * Producer side
 *
 * Publishes the producer buffer as the newest data. If the consumer has not
 * taken the previous one yet, that one is dropped.
 */
void *swapchain_producer_buffer(swapchain_t *this);
void swapchain_producer_swap(swapchain_t *this);

/**
 * Consumer side
 *
 * Takes the newest published buffer. Returns false, and keeps the current
 * consumer buffer, when nothing new was published since the last swap.
 */
const void *swapchain_consumer_buffer(swapchain_t *this);
bool swapchain_consumer_swap(swapchain_t *this);

void swapchain_deinit(swapchain_t *this);

#endif