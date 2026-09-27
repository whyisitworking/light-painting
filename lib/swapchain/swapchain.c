#include "swapchain.h"

// Set in shared while the shared buffer holds data the consumer has not
// taken yet, above the buffer indices
constexpr uint8_t FRESH = 1u << 7;
constexpr uint8_t INDEX = FRESH - 1;

static_assert(SWAPCHAIN_BUFFER_COUNT <= INDEX + 1u,
              "the buffer indices must fit below FRESH");

bool swapchain_init(swapchain_t *this, size_t buffer_size) {
    char *memory = (char *)malloc(SWAPCHAIN_BUFFER_COUNT * buffer_size);
    if (memory == nullptr)
        return false;

    for (size_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
        this->buffers[i] = memory + i * buffer_size;

    this->memory = memory;
    this->producer = 0;
    this->consumer = 2;
    atomic_init(&this->shared, 1);

    return true;
}

void *swapchain_producer_buffer(swapchain_t *this) {
    return this->buffers[this->producer];
}

void swapchain_producer_swap(swapchain_t *this) {
    // Release: the consumer sees everything written into the buffer.
    // Acquire: the buffer taken back was released by the consumer, which
    // no longer reads it
    uint8_t previous = atomic_exchange_explicit(
        &this->shared, this->producer | FRESH, memory_order_acq_rel);

    this->producer = previous & INDEX;
}

const void *swapchain_consumer_buffer(const swapchain_t *this) {
    return this->buffers[this->consumer];
}

bool swapchain_consumer_swap(swapchain_t *this) {
    uint8_t previous;

    // Swapping now would hand back our own previous, older, buffer. Only
    // the consumer clears FRESH, so once seen it is still set below, on
    // this or an even newer buffer
    if (!(atomic_load_explicit(&this->shared, memory_order_acquire) & FRESH))
        return false;

    previous = atomic_exchange_explicit(&this->shared, this->consumer,
                                        memory_order_acq_rel);
    this->consumer = previous & INDEX;

    return true;
}

void swapchain_deinit(swapchain_t *this) {
    free(this->memory);

    // No buffer pointer outlives the memory
    *this = (swapchain_t){0};
}
