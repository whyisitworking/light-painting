#ifndef TESTS_CHECK_H
#define TESTS_CHECK_H

/**
 * Minimal assertion helpers for the host-side unit tests.
 * Each test executable prints the number of failed checks and exits with
 * status 1 if there was any.
 */

#include <math.h>
#include <stdio.h>

static int check_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            check_failures++;                                                  \
            printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond);    \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                \
    do {                                                                       \
        double check_a = (actual), check_e = (expected);                       \
        if (!(fabs(check_a - check_e) <= (tolerance))) {                       \
            check_failures++;                                                  \
            printf("%s:%d: CHECK_NEAR(%s, %s) failed: %g vs %g\n", __FILE__,   \
                   __LINE__, #actual, #expected, check_a, check_e);            \
        }                                                                      \
    } while (0)

// Not the raw count: exit statuses are 8 bits, 256 failures would read as 0
#define CHECK_REPORT()                                                         \
    (printf("%s: %d failure(s)\n", __FILE__, check_failures),                 \
     check_failures ? 1 : 0)

#endif
