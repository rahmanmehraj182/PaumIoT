/**
 * @file test_framework.h
 * @brief Simple testing framework for PaumIoT
 * @details Provides macros and utilities for unit testing without external dependencies
 */

#ifndef PAUMIOT_TEST_FRAMEWORK_H
#define PAUMIOT_TEST_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Test statistics */
typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    int assertions_run;
    int assertions_passed;
    int assertions_failed;
} test_stats_t;

/* Global test statistics */
extern test_stats_t g_test_stats;

/* Color codes for output */
#define TEST_COLOR_RESET   "\033[0m"
#define TEST_COLOR_RED     "\033[31m"
#define TEST_COLOR_GREEN   "\033[32m"
#define TEST_COLOR_YELLOW  "\033[33m"
#define TEST_COLOR_BLUE    "\033[34m"
#define TEST_COLOR_MAGENTA "\033[35m"
#define TEST_COLOR_CYAN    "\033[36m"

/* Test macros */
#define TEST_INIT() \
    do { \
        memset(&g_test_stats, 0, sizeof(g_test_stats)); \
        printf(TEST_COLOR_CYAN "Starting test suite...\n" TEST_COLOR_RESET); \
    } while(0)

#define TEST_SUITE_BEGIN(suite_name) \
    printf(TEST_COLOR_BLUE "\n=== Test Suite: %s ===\n" TEST_COLOR_RESET, suite_name)

#define TEST_CASE(test_name) \
    do { \
        printf(TEST_COLOR_YELLOW "Running: %s... " TEST_COLOR_RESET, test_name); \
        fflush(stdout); \
        g_test_stats.tests_run++; \
        bool test_passed = true;

#define TEST_CASE_END() \
        if (test_passed) { \
            printf(TEST_COLOR_GREEN "PASSED\n" TEST_COLOR_RESET); \
            g_test_stats.tests_passed++; \
        } else { \
            printf(TEST_COLOR_RED "FAILED\n" TEST_COLOR_RESET); \
            g_test_stats.tests_failed++; \
        } \
    } while(0)

#define TEST_SUMMARY() \
    do { \
        printf(TEST_COLOR_CYAN "\n=== Test Summary ===\n" TEST_COLOR_RESET); \
        printf("Tests run: %d\n", g_test_stats.tests_run); \
        printf(TEST_COLOR_GREEN "Tests passed: %d\n" TEST_COLOR_RESET, g_test_stats.tests_passed); \
        if (g_test_stats.tests_failed > 0) { \
            printf(TEST_COLOR_RED "Tests failed: %d\n" TEST_COLOR_RESET, g_test_stats.tests_failed); \
        } \
        printf("Assertions run: %d\n", g_test_stats.assertions_run); \
        printf(TEST_COLOR_GREEN "Assertions passed: %d\n" TEST_COLOR_RESET, g_test_stats.assertions_passed); \
        if (g_test_stats.assertions_failed > 0) { \
            printf(TEST_COLOR_RED "Assertions failed: %d\n" TEST_COLOR_RESET, g_test_stats.assertions_failed); \
        } \
        if (g_test_stats.tests_failed == 0) { \
            printf(TEST_COLOR_GREEN "\nAll tests passed!\n" TEST_COLOR_RESET); \
        } else { \
            printf(TEST_COLOR_RED "\nSome tests failed!\n" TEST_COLOR_RESET); \
        } \
        return g_test_stats.tests_failed; \
    } while(0)

/* Assertion macros */
#define ASSERT_TRUE(condition) \
    do { \
        g_test_stats.assertions_run++; \
        if (condition) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: %s (line %d)\n" TEST_COLOR_RESET, \
                   #condition, __LINE__); \
            test_passed = false; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) \
    do { \
        g_test_stats.assertions_run++; \
        if ((expected) == (actual)) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: expected %ld, got %ld (line %d)\n" TEST_COLOR_RESET, \
                   (long)(expected), (long)(actual), __LINE__); \
            test_passed = false; \
        } \
    } while(0)

#define ASSERT_NEQ(not_expected, actual) \
    do { \
        g_test_stats.assertions_run++; \
        if ((not_expected) != (actual)) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: expected not %ld, got %ld (line %d)\n" TEST_COLOR_RESET, \
                   (long)(not_expected), (long)(actual), __LINE__); \
            test_passed = false; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        g_test_stats.assertions_run++; \
        if (strcmp((expected), (actual)) == 0) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: expected \"%s\", got \"%s\" (line %d)\n" TEST_COLOR_RESET, \
                   (expected), (actual), __LINE__); \
            test_passed = false; \
        } \
    } while(0)

#define ASSERT_NULL(pointer) \
    do { \
        g_test_stats.assertions_run++; \
        if ((pointer) == NULL) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: expected NULL, got %p (line %d)\n" TEST_COLOR_RESET, \
                   (void*)(pointer), __LINE__); \
            test_passed = false; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(pointer) \
    do { \
        g_test_stats.assertions_run++; \
        if ((pointer) != NULL) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: expected not NULL, got NULL (line %d)\n" TEST_COLOR_RESET, \
                   __LINE__); \
            test_passed = false; \
        } \
    } while(0)

#define ASSERT_SUCCESS(result) \
    do { \
        g_test_stats.assertions_run++; \
        if ((result) == PAUMIOT_SUCCESS) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: expected PAUMIOT_SUCCESS, got %d (line %d)\n" TEST_COLOR_RESET, \
                   (int)(result), __LINE__); \
            test_passed = false; \
        } \
    } while(0)

#define ASSERT_ERROR(result) \
    do { \
        g_test_stats.assertions_run++; \
        if ((result) != PAUMIOT_SUCCESS) { \
            g_test_stats.assertions_passed++; \
        } else { \
            g_test_stats.assertions_failed++; \
            printf(TEST_COLOR_RED "\n  ASSERTION FAILED: expected error, got PAUMIOT_SUCCESS (line %d)\n" TEST_COLOR_RESET, \
                   __LINE__); \
            test_passed = false; \
        } \
    } while(0)

/* Memory testing helpers */
typedef struct {
    void *ptr;
    size_t size;
    const char *file;
    int line;
} mem_allocation_t;

#define MAX_ALLOCATIONS 1000
extern mem_allocation_t g_allocations[MAX_ALLOCATIONS];
extern int g_allocation_count;

#define TEST_MALLOC(size) test_malloc(size, __FILE__, __LINE__)
#define TEST_FREE(ptr) test_free(ptr)

void *test_malloc(size_t size, const char *file, int line);
void test_free(void *ptr);
void test_check_leaks(void);
void test_reset_allocations(void);

/* Utility functions */
void test_print_hex(const uint8_t *data, size_t len);
bool test_compare_buffers(const uint8_t *buf1, const uint8_t *buf2, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PAUMIOT_TEST_FRAMEWORK_H */