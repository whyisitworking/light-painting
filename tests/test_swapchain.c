#include "check.h"
#include "swapchain.h"

#include <stdint.h>

static void publish(swapchain_t *chain, uint32_t value) {
    *(uint32_t *)swapchain_producer_buffer(chain) = value;
    swapchain_producer_swap(chain);
}

static uint32_t consumed(swapchain_t *chain) {
    return *(const uint32_t *)swapchain_consumer_buffer(chain);
}

static void test_buffers_are_distinct(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(uint32_t)));

    for (int i = 0; i < 10; i++) {
        CHECK(swapchain_producer_buffer(&chain) !=
              swapchain_consumer_buffer(&chain));
        publish(&chain, i);
        swapchain_consumer_swap(&chain);
    }

    swapchain_deinit(&chain);
}

static void test_nothing_published(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(uint32_t)));
    CHECK(!swapchain_consumer_swap(&chain));
    CHECK(chain.dropped == 0);

    swapchain_deinit(&chain);
}

static void test_in_order(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(uint32_t)));

    for (uint32_t i = 1; i <= 5; i++) {
        publish(&chain, i);
        CHECK(swapchain_consumer_swap(&chain));
        CHECK(consumed(&chain) == i);
    }

    CHECK(chain.dropped == 0);

    swapchain_deinit(&chain);
}

// Swapping without new data used to hand the consumer an older buffer
static void test_never_goes_back_in_time(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(uint32_t)));

    publish(&chain, 1);
    CHECK(swapchain_consumer_swap(&chain));
    publish(&chain, 2);
    CHECK(swapchain_consumer_swap(&chain));
    CHECK(consumed(&chain) == 2);

    // No new data: keep 2, not 1
    CHECK(!swapchain_consumer_swap(&chain));
    CHECK(consumed(&chain) == 2);

    swapchain_deinit(&chain);
}

static void test_newest_wins_and_drops_counted(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(uint32_t)));

    publish(&chain, 1);
    publish(&chain, 2);
    publish(&chain, 3);

    CHECK(swapchain_consumer_swap(&chain));
    CHECK(consumed(&chain) == 3);
    CHECK(chain.dropped == 2);

    swapchain_deinit(&chain);
}

static void test_aligned_buffers(void) {
    const size_t sizes[] = {1, 100, 2048, 3000};
    const size_t alignments[] = {1, 4, 64, 2048};

    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        for (size_t a = 0; a < sizeof(alignments) / sizeof(alignments[0]);
             a++) {
            size_t size = sizes[s], alignment = alignments[a];
            uintptr_t buffers[3];
            swapchain_t chain;

            CHECK(swapchain_init_aligned(&chain, size, alignment));

            buffers[0] = (uintptr_t)swapchain_producer_buffer(&chain);
            buffers[1] = (uintptr_t)chain.buffer_chain[1];
            buffers[2] = (uintptr_t)swapchain_consumer_buffer(&chain);

            for (int i = 0; i < 3; i++) {
                CHECK(buffers[i] % alignment == 0);

                // Never overlapping one another
                for (int j = 0; j < 3; j++)
                    if (i != j)
                        CHECK(buffers[i] + size <= buffers[j] ||
                              buffers[j] + size <= buffers[i]);
            }

            // Writable end to end (sanitizers would catch an overrun)
            for (int i = 0; i < 3; i++)
                for (size_t k = 0; k < size; k++)
                    ((volatile uint8_t *)buffers[i])[k] = (uint8_t)k;

            swapchain_deinit(&chain);
        }
    }
}

static void test_aligned_rejects_bad_alignment(void) {
    swapchain_t chain;

    CHECK(!swapchain_init_aligned(&chain, 16, 0));
    CHECK(!swapchain_init_aligned(&chain, 16, 3));
    CHECK(!swapchain_init_aligned(&chain, 16, 48));
}

int main(void) {
    test_aligned_buffers();
    test_aligned_rejects_bad_alignment();
    test_buffers_are_distinct();
    test_nothing_published();
    test_in_order();
    test_never_goes_back_in_time();
    test_newest_wins_and_drops_counted();

    return CHECK_REPORT();
}
