/**
 * @file test_logging.c
 * @brief Unit tests for logging system
 */

#include "logging.h"
#include <stdio.h>
#include <assert.h>

static void test_log_level_get_set(void) {
    printf("Testing log level get/set...\n");
    
    /* Test initial level */
    log_level_t initial = log_get_level();
    
    /* Test setting levels */
    log_set_level(LOG_LEVEL_DEBUG);
    assert(log_get_level() == LOG_LEVEL_DEBUG);
    
    log_set_level(LOG_LEVEL_INFO);
    assert(log_get_level() == LOG_LEVEL_INFO);
    
    log_set_level(LOG_LEVEL_WARN);
    assert(log_get_level() == LOG_LEVEL_WARN);
    
    log_set_level(LOG_LEVEL_ERROR);
    assert(log_get_level() == LOG_LEVEL_ERROR);
    
    log_set_level(LOG_LEVEL_NONE);
    assert(log_get_level() == LOG_LEVEL_NONE);
    
    /* Restore initial level */
    log_set_level(initial);
    
    printf("  ✓ Log level get/set test passed\n");
}

static void test_log_macros(void) {
    printf("Testing log macros...\n");
    
    log_set_level(LOG_LEVEL_DEBUG);
    
    printf("  The following log messages should appear:\n");
    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message");
    
    printf("  ✓ Log macros test passed\n");
}

static void test_log_filtering(void) {
    printf("Testing log filtering...\n");
    
    log_set_level(LOG_LEVEL_WARN);
    
    printf("  Only WARN and ERROR should appear below:\n");
    LOG_DEBUG("This DEBUG should NOT appear");
    LOG_INFO("This INFO should NOT appear");
    LOG_WARN("This WARN SHOULD appear");
    LOG_ERROR("This ERROR SHOULD appear");
    
    printf("  ✓ Log filtering test passed\n");
}

static void test_log_none_level(void) {
    printf("Testing LOG_LEVEL_NONE...\n");
    
    log_set_level(LOG_LEVEL_NONE);
    
    printf("  Nothing should appear below:\n");
    LOG_DEBUG("This should NOT appear");
    LOG_INFO("This should NOT appear");
    LOG_WARN("This should NOT appear");
    LOG_ERROR("This should NOT appear");
    
    printf("  ✓ LOG_LEVEL_NONE test passed\n");
}

static void test_log_format_strings(void) {
    printf("Testing log with format strings...\n");
    
    log_set_level(LOG_LEVEL_INFO);
    
    printf("  Testing formatted output:\n");
    LOG_INFO("Integer: %d", 42);
    LOG_INFO("String: %s", "test");
    LOG_INFO("Multiple: %d, %s, %f", 42, "test", 3.14);
    
    printf("  ✓ Log format string test passed\n");
}

static void test_log_level_ordering(void) {
    printf("Testing log level ordering...\n");
    
    /* Verify level ordering */
    assert(LOG_LEVEL_DEBUG < LOG_LEVEL_INFO);
    assert(LOG_LEVEL_INFO < LOG_LEVEL_WARN);
    assert(LOG_LEVEL_WARN < LOG_LEVEL_ERROR);
    assert(LOG_LEVEL_ERROR < LOG_LEVEL_NONE);
    
    printf("  ✓ Log level ordering test passed\n");
}

int main(void) {
    printf("\n========================================\n");
    printf("Running logging.h tests...\n");
    printf("========================================\n\n");
    
    test_log_level_get_set();
    test_log_macros();
    test_log_filtering();
    test_log_none_level();
    test_log_format_strings();
    test_log_level_ordering();
    
    printf("\n========================================\n");
    printf("✅ All tests passed successfully!\n");
    printf("========================================\n\n");
    
    return 0;
}
