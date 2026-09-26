#include "check.h"
#include "fft.h"

static void test_init(void) {
    fft_t fft;

    CHECK(fft_init(&fft, 64) == 1);
    CHECK(fft.count == 64);

    fft_deinit(&fft);
}

int main(void) {
    test_init();

    return CHECK_REPORT();
}
