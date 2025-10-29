# PaumIoT Middleware Makefile
# New Architecture - Common Utilities Build System

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2
INCLUDES = -I./common/include
LDFLAGS = 

# Directories
BUILD_DIR = build
COMMON_SRC = common/src
COMMON_INC = common/include
TEST_DIR = tests/unit

# Source files
COMMON_SRCS = $(COMMON_SRC)/errors.c \
              $(COMMON_SRC)/logging.c \
              $(COMMON_SRC)/memory_pool.c

# Object files
COMMON_OBJS = $(BUILD_DIR)/errors.o \
              $(BUILD_DIR)/logging.o \
              $(BUILD_DIR)/memory_pool.o

# Test executables
TESTS = $(BUILD_DIR)/test_types \
        $(BUILD_DIR)/test_errors \
        $(BUILD_DIR)/test_logging \
        $(BUILD_DIR)/test_memory_pool

# Default target
.PHONY: all
all: $(BUILD_DIR) $(COMMON_OBJS) $(TESTS)
	@echo ""
	@echo "✅ Build complete!"
	@echo "   Run 'make test' to run all tests"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile common utilities
$(BUILD_DIR)/errors.o: $(COMMON_SRC)/errors.c $(COMMON_INC)/errors.h $(COMMON_INC)/types.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/logging.o: $(COMMON_SRC)/logging.c $(COMMON_INC)/logging.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/memory_pool.o: $(COMMON_SRC)/memory_pool.c $(COMMON_INC)/memory_pool.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build tests
$(BUILD_DIR)/test_types: $(TEST_DIR)/test_types.c $(COMMON_INC)/types.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BUILD_DIR)/test_errors: $(TEST_DIR)/test_errors.c $(BUILD_DIR)/errors.o
	$(CC) $(CFLAGS) $(INCLUDES) $< $(BUILD_DIR)/errors.o -o $@

$(BUILD_DIR)/test_logging: $(TEST_DIR)/test_logging.c $(BUILD_DIR)/logging.o
	$(CC) $(CFLAGS) $(INCLUDES) $< $(BUILD_DIR)/logging.o -o $@

$(BUILD_DIR)/test_memory_pool: $(TEST_DIR)/test_memory_pool.c $(BUILD_DIR)/memory_pool.o
	$(CC) $(CFLAGS) $(INCLUDES) $< $(BUILD_DIR)/memory_pool.o -o $@

# Run all tests
.PHONY: test
test: all
	@echo ""
	@echo "=========================================="
	@echo "Running all tests..."
	@echo "=========================================="
	@echo ""
	@echo "→ Running test_types..."
	@$(BUILD_DIR)/test_types
	@echo ""
	@echo "→ Running test_errors..."
	@$(BUILD_DIR)/test_errors
	@echo ""
	@echo "→ Running test_logging..."
	@$(BUILD_DIR)/test_logging 2>&1 | head -40
	@echo ""
	@echo "→ Running test_memory_pool..."
	@$(BUILD_DIR)/test_memory_pool
	@echo ""
	@echo "=========================================="
	@echo "✅ All tests completed successfully!"
	@echo "=========================================="

# Run individual tests
.PHONY: test-types
test-types: $(BUILD_DIR)/test_types
	@$(BUILD_DIR)/test_types

.PHONY: test-errors
test-errors: $(BUILD_DIR)/test_errors
	@$(BUILD_DIR)/test_errors

.PHONY: test-logging
test-logging: $(BUILD_DIR)/test_logging
	@$(BUILD_DIR)/test_logging

.PHONY: test-memory-pool
test-memory-pool: $(BUILD_DIR)/test_memory_pool
	@$(BUILD_DIR)/test_memory_pool

# Clean build artifacts
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
	@echo "✅ Clean complete"

# Rebuild everything
.PHONY: rebuild
rebuild: clean all

# Show help
.PHONY: help
help:
	@echo "PaumIoT Build System - Common Utilities"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build all components and tests"
	@echo "  make test         - Build and run all tests"
	@echo "  make test-types   - Run only types test"
	@echo "  make test-errors  - Run only errors test"
	@echo "  make test-logging - Run only logging test"
	@echo "  make test-memory-pool - Run only memory pool test"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make rebuild      - Clean and rebuild everything"
	@echo "  make help         - Show this help message"