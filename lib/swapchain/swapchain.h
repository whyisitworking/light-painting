#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

/**
 * Swapchain: a triple buffer handing data from a producer to a consumer.
 *
 *   producer ──fills──► [producer] ⇄ [shared] ⇄ [consumer] ──reads──► consumer
 *
 * Each side owns one buffer and swaps it with the shared one: the producer
 * never waits and never overwrites what the consumer reads, and the
 * consumer always gets the newest complete buffer. A buffer the consumer
 * missed is simply replaced. Nothing is copied, only buffer indices swap.
 *
 * Each swap is one atomic exchange of the shared index, so neither side
 * ever waits: they may run on different cores, or one in an interrupt
 * handler, with no lock and no interrupts masked.
 */

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

// A producer, a shared and a consumer buffer
constexpr size_t SWAPCHAIN_BUFFER_COUNT = 3;

typedef struct {
    // This amazing quote by Herb Sutter guarantees correct alignment for
    // arbitrary data.
    //      "Alignment. Any memory Alignment. Any memory that's allocated
    //      dynamically via new or malloc is guaranteed to be properly aligned
    //      for objects of **any type**, but buffers that are not allocated
    //      dynamically have no such guarantee."
    void *memory;
    void *buffers[SWAPCHAIN_BUFFER_COUNT];
    // The buffer each side owns, only ever touched by that side
    uint8_t producer;
    uint8_t consumer;
    // The shared buffer, plus a flag while it holds data the consumer has
    // not taken yet. Both sides exchange it atomically
    _Atomic uint8_t shared;
} swapchain_t;

/**
 * Allocates SWAPCHAIN_BUFFER_COUNT buffers of buffer_size bytes, for one
 * producer and one consumer that may run in different contexts: an
 * interrupt handler and the main loop, or the two cores.
 */
[[nodiscard]] bool swapchain_init(swapchain_t *this, size_t buffer_size);

/**
 * Producer side
 *
 * Publishes the producer buffer as the newest data. If the consumer has not
 * taken the previous one yet, it never will.
 */
void *swapchain_producer_buffer(swapchain_t *this);
void swapchain_producer_swap(swapchain_t *this);

/**
 * Consumer side
 *
 * Takes the newest published buffer. Returns false, and keeps the current
 * consumer buffer, when nothing new was published since the last swap.
 */
const void *swapchain_consumer_buffer(const swapchain_t *this);
bool swapchain_consumer_swap(swapchain_t *this);

// Frees the buffers, the buffer getters return nullptr afterwards
void swapchain_deinit(swapchain_t *this);

#endif