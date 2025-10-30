/**
 * @file test_errors.c
 * @brief Unit tests for error system
 */

#include "errors.h"
#include "types.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_error_strings(void) {
    printf("Testing error strings...\n");
    
    /* Test some common error codes */
    const char* success_str = paumiot_error_string(PAUMIOT_SUCCESS);
    assert(success_str != NULL);
    assert(strcmp(success_str, "Success") == 0);
    
    const char* invalid_param_str = paumiot_error_string(PAUMIOT_ERROR_INVALID_PARAM);
    assert(invalid_param_str != NULL);
    assert(strlen(invalid_param_str) > 0);
    
    const char* oom_str = paumiot_error_string(PAUMIOT_ERROR_OUT_OF_MEMORY);
    assert(oom_str != NULL);
    assert(strlen(oom_str) > 0);
    
    printf("  ✓ Error strings test passed\n");
}

static void test_all_error_codes_have_strings(void) {
    printf("Testing all error codes have strings...\n");
    
    int error_codes[] = {
        PAUMIOT_SUCCESS,
        PAUMIOT_ERROR_INVALID_PARAM,
        PAUMIOT_ERROR_NULL_POINTER,
        PAUMIOT_ERROR_OUT_OF_MEMORY,
        PAUMIOT_ERROR_BUFFER_TOO_SMALL,
        PAUMIOT_ERROR_QUEUE_FULL,
        PAUMIOT_ERROR_QUEUE_EMPTY,
        PAUMIOT_ERROR_NOT_FOUND,
        PAUMIOT_ERROR_ALREADY_EXISTS,
        PAUMIOT_ERROR_NOT_INITIALIZED,
        PAUMIOT_ERROR_INVALID_STATE,
        PAUMIOT_ERROR_CONNECTION_LOST,
        PAUMIOT_ERROR_CONNECTION_REFUSED,
        PAUMIOT_ERROR_TIMEOUT,
        PAUMIOT_ERROR_PROTOCOL_ERROR,
        PAUMIOT_ERROR_PROTOCOL_VERSION,
        PAUMIOT_ERROR_MALFORMED_PACKET,
        PAUMIOT_ERROR_NOT_SUPPORTED,
        PAUMIOT_ERROR_PERMISSION_DENIED,
        PAUMIOT_ERROR_OPERATION_FAILED,
        PAUMIOT_ERROR_INTERNAL,
        PAUMIOT_ERROR_UNKNOWN
    };
    
    int count = sizeof(error_codes) / sizeof(error_codes[0]);
    
    for (int i = 0; i < count; i++) {
        const char* str = paumiot_error_string(error_codes[i]);
        assert(str != NULL);
        assert(strlen(str) > 0);
    }
    
    printf("  ✓ All %d error codes have strings\n", count - 1); /* -1 for SUCCESS */
}

static void test_unknown_error_code(void) {
    printf("Testing unknown error code...\n");
    
    const char* unknown_str = paumiot_error_string(-999);
    assert(unknown_str != NULL);
    assert(strcmp(unknown_str, "Unrecognized error code") == 0);
    
    printf("  ✓ Unknown error code test passed\n");
}

static void test_is_success_helper(void) {
    printf("Testing is_success helper...\n");
    
    assert(paumiot_is_success(PAUMIOT_SUCCESS) == true);
    assert(paumiot_is_success(PAUMIOT_ERROR_INVALID_PARAM) == false);
    assert(paumiot_is_success(PAUMIOT_ERROR_OUT_OF_MEMORY) == false);
    assert(paumiot_is_success(PAUMIOT_ERROR_TIMEOUT) == false);
    
    printf("  ✓ is_success helper test passed\n");
}

static void test_is_error_helper(void) {
    printf("Testing is_error helper...\n");
    
    assert(paumiot_is_error(PAUMIOT_SUCCESS) == false);
    assert(paumiot_is_error(PAUMIOT_ERROR_INVALID_PARAM) == true);
    assert(paumiot_is_error(PAUMIOT_ERROR_OUT_OF_MEMORY) == true);
    assert(paumiot_is_error(PAUMIOT_ERROR_TIMEOUT) == true);
    
    printf("  ✓ is_error helper test passed\n");
}

int main(void) {
    printf("\n========================================\n");
    printf("Running errors.h tests...\n");
    printf("========================================\n\n");
    
    test_error_strings();
    test_all_error_codes_have_strings();
    test_unknown_error_code();
    test_is_success_helper();
    test_is_error_helper();
    
    printf("\n========================================\n");
    printf("✅ All tests passed successfully!\n");
    printf("========================================\n\n");
    
    return 0;
}
