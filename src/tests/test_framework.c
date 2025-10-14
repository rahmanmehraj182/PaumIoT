/**
 * @file test_framework.c
 * @brief Implementation of the simple testing framework
 */

#include "../tests/test_framework.h"

/* Global test statistics */
test_stats_t g_test_stats = {0};

/* Memory tracking for leak detection */
mem_allocation_t g_allocations[MAX_ALLOCATIONS] = {0};
int g_allocation_count = 0;

void *test_malloc(size_t size, const char *file, int line) {
    void *ptr = malloc(size);
    if (ptr && g_allocation_count < MAX_ALLOCATIONS) {
        g_allocations[g_allocation_count].ptr = ptr;
        g_allocations[g_allocation_count].size = size;
        g_allocations[g_allocation_count].file = file;
        g_allocations[g_allocation_count].line = line;
        g_allocation_count++;
    }
    return ptr;
}

void test_free(void *ptr) {
    if (!ptr) return;
    
    /* Find and remove allocation record */
    for (int i = 0; i < g_allocation_count; i++) {
        if (g_allocations[i].ptr == ptr) {
            /* Move last entry to this position */
            g_allocations[i] = g_allocations[g_allocation_count - 1];
            g_allocation_count--;
            break;
        }
    }
    
    free(ptr);
}

void test_check_leaks(void) {
    if (g_allocation_count > 0) {
        printf(TEST_COLOR_RED "\n=== Memory Leaks Detected ===\n" TEST_COLOR_RESET);
        for (int i = 0; i < g_allocation_count; i++) {
            printf("  Leak: %zu bytes allocated at %s:%d\n",
                   g_allocations[i].size,
                   g_allocations[i].file,
                   g_allocations[i].line);
        }
    } else {
        printf(TEST_COLOR_GREEN "No memory leaks detected\n" TEST_COLOR_RESET);
    }
}

void test_reset_allocations(void) {
    g_allocation_count = 0;
    memset(g_allocations, 0, sizeof(g_allocations));
}

void test_print_hex(const uint8_t *data, size_t len) {
    printf("  ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n  ");
        }
    }
    if (len % 16 != 0) {
        printf("\n");
    }
}

bool test_compare_buffers(const uint8_t *buf1, const uint8_t *buf2, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf1[i] != buf2[i]) {
            printf("  Buffer mismatch at offset %zu: expected 0x%02X, got 0x%02X\n",
                   i, buf1[i], buf2[i]);
            return false;
        }
    }
    return true;
}