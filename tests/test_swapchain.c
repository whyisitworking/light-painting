#include "check.h"
#include "swapchain.h"

#include <pthread.h>
#include <stdint.h>

// Buffers the stress test publishes, and the words in each
constexpr uint32_t STRESS_COUNT = 1'000'000;
constexpr size_t STRESS_WORDS = 64;

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

static void test_newest_wins(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(uint32_t)));

    publish(&chain, 1);
    publish(&chain, 2);
    publish(&chain, 3);

    CHECK(swapchain_consumer_swap(&chain));
    CHECK(consumed(&chain) == 3);

    swapchain_deinit(&chain);
}

// Nothing points into the freed buffers, e.g. after a driver failed to init
static void test_deinit_forgets_buffers(void) {
    swapchain_t chain;

    CHECK(swapchain_init(&chain, sizeof(int)));
    publish(&chain, 1);
    swapchain_deinit(&chain);

    CHECK(swapchain_producer_buffer(&chain) == nullptr);
    CHECK(swapchain_consumer_buffer(&chain) == nullptr);
    CHECK(!swapchain_consumer_swap(&chain));
}

// Publishes STRESS_COUNT buffers as fast as it can, each filled with its
// sequence number
static void *stress_producer(void *chain) {
    for (uint32_t sequence = 1; sequence <= STRESS_COUNT; sequence++) {
        uint32_t *words = (uint32_t *)swapchain_producer_buffer(chain);

        for (size_t i = 0; i < STRESS_WORDS; i++)
            words[i] = sequence;

        swapchain_producer_swap(chain);
    }

    return nullptr;
}

// The two sides on two threads at full speed, as on the two cores: every
// buffer taken is whole, never torn, and newer than the one before
static void test_two_threads(void) {
    swapchain_t chain;
    pthread_t producer;
    uint32_t last = 0;
    size_t taken = 0, torn = 0, older = 0;

    CHECK(swapchain_init(&chain, STRESS_WORDS * sizeof(uint32_t)));
    CHECK(pthread_create(&producer, nullptr, stress_producer, &chain) == 0);

    // The last buffer stays fresh until taken, so this ends
    while (last < STRESS_COUNT) {
        const uint32_t *words;

        if (!swapchain_consumer_swap(&chain))
            continue;

        words = (const uint32_t *)swapchain_consumer_buffer(&chain);

        for (size_t i = 1; i < STRESS_WORDS; i++)
            if (words[i] != words[0])
                torn++;

        if (words[0] <= last)
            older++;

        last = words[0];
        taken++;
    }

    CHECK(pthread_join(producer, nullptr) == 0);
    CHECK(torn == 0);
    CHECK(older == 0);
    CHECK(taken > 1);

    swapchain_deinit(&chain);
}

int main(void) {
    test_buffers_are_distinct();
    test_nothing_published();
    test_in_order();
    test_never_goes_back_in_time();
    test_newest_wins();
    test_deinit_forgets_buffers();
    test_two_threads();

    return CHECK_REPORT();
}
