/*
 * test_util.h — minimal, dependency-free test harness for FluckFlock host tests.
 *
 * Usage:
 *   Include once per test binary.
 *   Call ASSERT(expr) or ASSERT_EQ(a, b) anywhere; on failure the process exits
 *   immediately with a diagnostic message.  Call PASS("test_name") to print a
 *   confirmation line.  g_assert_count accumulates across all ASSERT / ASSERT_EQ
 *   calls and can be reported at the end of main().
 *
 * Each test binary is a single translation unit so `static int g_assert_count`
 * is safe here — no multiple-definition issue.
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>

/* Running count of checked assertions in this binary. */
static int g_assert_count = 0;

/*
 * ASSERT(expr)
 *   Fail loudly with file/line if expr is false.
 */
#define ASSERT(expr)                                                         \
    do {                                                                     \
        g_assert_count++;                                                    \
        if (!(expr)) {                                                       \
            fprintf(stderr, "FAIL  %s:%d  assertion failed: %s\n",          \
                    __FILE__, __LINE__, #expr);                              \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

/*
 * ASSERT_EQ(a, b)
 *   Fail if a != b.  Both must be scalar (compared with !=).
 */
#define ASSERT_EQ(a, b)                                                      \
    do {                                                                     \
        g_assert_count++;                                                    \
        if ((a) != (b)) {                                                    \
            fprintf(stderr, "FAIL  %s:%d  expected (%s) == (%s)\n",         \
                    __FILE__, __LINE__, #a, #b);                             \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

/*
 * PASS(name)
 *   Print a green-ish confirmation line.  Call once per test function on
 *   success (i.e., at the bottom of the function — reaching it means all
 *   assertions in that function passed).
 */
#define PASS(name) \
    printf("  PASS  %s\n", (name))
