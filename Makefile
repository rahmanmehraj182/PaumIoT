# PaumIoT Middleware Makefile
# IoT Protocol Middleware with MQTT/CoAP Adapters

# Project configuration
PROJECT_NAME := paumiot
VERSION := 1.0.0
BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIR := include
LIB_DIR := lib
TEST_DIR := tests
CONFIG_DIR := config

# Compiler settings
CC := gcc
CXX := g++
CFLAGS := -std=c99 -Wall -Wextra -Werror -pedantic
CXXFLAGS := -std=c++11 -Wall -Wextra -Werror -pedantic
INCLUDES := -I$(INCLUDE_DIR) -I$(SRC_DIR)
LDFLAGS := -L$(LIB_DIR)

# Debug and Release configurations
DEBUG_FLAGS := -g -DDEBUG -O0
RELEASE_FLAGS := -O2 -DNDEBUG

# Architecture-specific settings
ARCH ?= x86_64

# ARM cross-compilation (for embedded targets like ESP32, STM32)
ifeq ($(ARCH),arm)
    CC := arm-linux-gnueabihf-gcc
    CXX := arm-linux-gnueabihf-g++
    CFLAGS += -march=armv7-a -mfpu=neon
endif

# ESP32 cross-compilation
ifeq ($(ARCH),esp32)
    CC := xtensa-esp32-elf-gcc
    CXX := xtensa-esp32-elf-g++
    CFLAGS += -DESP32_TARGET
    INCLUDES += -I$(ESP_IDF)/components/freertos/include
endif

# STM32 cross-compilation
ifeq ($(ARCH),stm32)
    CC := arm-none-eabi-gcc
    CXX := arm-none-eabi-g++
    CFLAGS += -mcpu=cortex-m4 -mthumb -DSTM32_TARGET
endif

# Build mode
MODE ?= debug
ifeq ($(MODE),release)
    CFLAGS += $(RELEASE_FLAGS)
    CXXFLAGS += $(RELEASE_FLAGS)
else
    CFLAGS += $(DEBUG_FLAGS)
    CXXFLAGS += $(DEBUG_FLAGS)
endif

# Source files by layer
SMP_SOURCES := $(wildcard $(SRC_DIR)/smp/*/*.c)
PDL_SOURCES := $(wildcard $(SRC_DIR)/pdl/*/*.c)
PAL_SOURCES := $(wildcard $(SRC_DIR)/pal/*/*.c) $(wildcard $(SRC_DIR)/pal/*/*/*.c)
BLL_SOURCES := $(wildcard $(SRC_DIR)/bll/*/*.c)
DAL_SOURCES := $(wildcard $(SRC_DIR)/dal/*/*.c)

ALL_SOURCES := $(SMP_SOURCES) $(PDL_SOURCES) $(PAL_SOURCES) $(BLL_SOURCES) $(DAL_SOURCES)
OBJECTS := $(ALL_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Library dependencies
LIBS := -lpthread -lm
# Add protocol-specific libraries conditionally
ifdef USE_PAHO_MQTT
    LIBS += -lpaho-mqtt3c
    CFLAGS += -DUSE_PAHO_MQTT
endif

ifdef USE_LIBCOAP
    LIBS += -lcoap-2
    CFLAGS += -DUSE_LIBCOAP
endif

ifdef USE_MBEDTLS
    LIBS += -lmbedtls -lmbedx509 -lmbedcrypto
    CFLAGS += -DUSE_MBEDTLS
endif

# Target executable
TARGET := $(BUILD_DIR)/$(PROJECT_NAME)

# Test executable
TEST_TARGET := $(BUILD_DIR)/test_$(PROJECT_NAME)
TEST_SOURCES := $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS := $(TEST_SOURCES:$(TEST_DIR)/%.c=$(BUILD_DIR)/test/%.o)

.PHONY: all clean test install uninstall help docs docker

# Default target
all: $(TARGET)

# Build main executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@
	@echo "Built $(TARGET) for $(ARCH) architecture in $(MODE) mode"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build tests
test: $(TEST_TARGET)
	@echo "Running tests..."
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS) $(filter-out $(BUILD_DIR)/main.o, $(OBJECTS))
	@mkdir -p $(dir $@)
	$(CC) $(TEST_OBJECTS) $(filter-out $(BUILD_DIR)/main.o, $(OBJECTS)) $(LDFLAGS) $(LIBS) -o $@

$(BUILD_DIR)/test/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Architecture-specific builds
x86_64:
	$(MAKE) ARCH=x86_64

arm:
	$(MAKE) ARCH=arm

esp32:
	$(MAKE) ARCH=esp32

stm32:
	$(MAKE) ARCH=stm32

# Build modes
debug:
	$(MAKE) MODE=debug

release:
	$(MAKE) MODE=release

# Static analysis
static-analysis:
	@echo "Running static analysis..."
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)

# Memory check (requires valgrind)
memcheck: $(TARGET)
	@echo "Running memory check..."
	valgrind --tool=memcheck --leak-check=full ./$(TARGET)

# Protocol-specific builds
mqtt-only:
	$(MAKE) USE_PAHO_MQTT=1 CFLAGS="$(CFLAGS) -DMQTT_ONLY"

coap-only:
	$(MAKE) USE_LIBCOAP=1 CFLAGS="$(CFLAGS) -DCOAP_ONLY"

secure:
	$(MAKE) USE_MBEDTLS=1 CFLAGS="$(CFLAGS) -DSECURE_BUILD"

# Installation targets
install: $(TARGET)
	@echo "Installing $(PROJECT_NAME)..."
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/
	install -d /usr/local/include/$(PROJECT_NAME)
	cp -r $(INCLUDE_DIR)/* /usr/local/include/$(PROJECT_NAME)/
	@echo "Installation complete"

uninstall:
	@echo "Uninstalling $(PROJECT_NAME)..."
	rm -f /usr/local/bin/$(PROJECT_NAME)
	rm -rf /usr/local/include/$(PROJECT_NAME)
	@echo "Uninstallation complete"

# Docker support
docker-build:
	docker build -t $(PROJECT_NAME):$(VERSION) .

docker-run:
	docker run --rm -it $(PROJECT_NAME):$(VERSION)

# Documentation generation
docs:
	@echo "Generating documentation..."
	doxygen $(CONFIG_DIR)/Doxyfile

# Clean targets
clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

clean-docs:
	rm -rf docs/html docs/latex

distclean: clean clean-docs

# Development helpers
format:
	@echo "Formatting source code..."
	find $(SRC_DIR) $(INCLUDE_DIR) $(TEST_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

lint:
	@echo "Running linter..."
	find $(SRC_DIR) $(INCLUDE_DIR) $(TEST_DIR) -name "*.c" -o -name "*.h" | xargs cpplint

# Benchmarking
benchmark: $(TARGET)
	@echo "Running performance benchmarks..."
	./scripts/benchmark.sh $(TARGET)

# Code coverage (requires gcov)
coverage:
	$(MAKE) CFLAGS="$(CFLAGS) --coverage" LDFLAGS="$(LDFLAGS) --coverage"
	./$(TEST_TARGET)
	gcov $(OBJECTS)

# Help target
help:
	@echo "PaumIoT Middleware Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build the main executable (default)"
	@echo "  test         - Build and run tests"
	@echo "  clean        - Clean build artifacts"
	@echo "  install      - Install to system"
	@echo "  uninstall    - Remove from system"
	@echo ""
	@echo "Architecture targets:"
	@echo "  x86_64       - Build for x86_64 (default)"
	@echo "  arm          - Cross-compile for ARM"
	@echo "  esp32        - Cross-compile for ESP32"
	@echo "  stm32        - Cross-compile for STM32"
	@echo ""
	@echo "Build modes:"
	@echo "  debug        - Debug build (default)"
	@echo "  release      - Release build"
	@echo ""
	@echo "Protocol builds:"
	@echo "  mqtt-only    - MQTT-only build"
	@echo "  coap-only    - CoAP-only build"
	@echo "  secure       - Build with security features"
	@echo ""
	@echo "Development:"
	@echo "  format       - Format source code"
	@echo "  lint         - Run linter"
	@echo "  memcheck     - Run memory checks"
	@echo "  coverage     - Generate code coverage"
	@echo "  benchmark    - Run performance benchmarks"
	@echo "  docs         - Generate documentation"
	@echo ""
	@echo "Variables:"
	@echo "  ARCH={x86_64|arm|esp32|stm32} - Target architecture"
	@echo "  MODE={debug|release}           - Build mode"

# Print build configuration
config:
	@echo "Build Configuration:"
	@echo "  Project:      $(PROJECT_NAME) v$(VERSION)"
	@echo "  Architecture: $(ARCH)"
	@echo "  Mode:         $(MODE)"
	@echo "  Compiler:     $(CC)"
	@echo "  CFLAGS:       $(CFLAGS)"
	@echo "  INCLUDES:     $(INCLUDES)"
	@echo "  LIBS:         $(LIBS)"