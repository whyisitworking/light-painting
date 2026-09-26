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

// Nothing points into the freed buffers, e.g. after a driver failed to init
static void test_deinit_forgets_buffers(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(int)));
    publish(&chain, 1);
    swapchain_deinit(&chain);

    CHECK(swapchain_producer_buffer(&chain) == NULL);
    CHECK(swapchain_consumer_buffer(&chain) == NULL);
    CHECK(!swapchain_consumer_swap(&chain));
}

int main(void) {
    test_buffers_are_distinct();
    test_nothing_published();
    test_in_order();
    test_never_goes_back_in_time();
    test_newest_wins_and_drops_counted();
    test_deinit_forgets_buffers();

    return CHECK_REPORT();
}
