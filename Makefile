# PaumIoT - Protocol-Agnostic Universal Middleware for IoT
# Makefile for building the multi-protocol server

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -pthread -g
INCLUDES = -Iinclude
TARGET = paumiot
BUILDDIR = build
SRCDIR = src

# Source files
CORE_SOURCES = $(SRCDIR)/core/server_core.c
PROTOCOL_SOURCES = $(SRCDIR)/protocol/protocol_detector.c $(SRCDIR)/protocol/protocol_handlers.c
NETWORK_SOURCES = $(SRCDIR)/network/network_manager.c
SESSION_SOURCES = $(SRCDIR)/session/session_manager.c
MAIN_SOURCE = main.c

# All sources
SOURCES = $(CORE_SOURCES) $(PROTOCOL_SOURCES) $(NETWORK_SOURCES) $(SESSION_SOURCES) $(MAIN_SOURCE)

# Object files
OBJECTS = $(SOURCES:%.c=$(BUILDDIR)/%.o)

# Default target
all: $(TARGET)

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)
	mkdir -p $(BUILDDIR)/src/core
	mkdir -p $(BUILDDIR)/src/protocol
	mkdir -p $(BUILDDIR)/src/network
	mkdir -p $(BUILDDIR)/src/session

# Build target
$(TARGET): $(BUILDDIR) $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(CFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile source files
$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILDDIR)
	rm -f $(TARGET)
	@echo "Clean complete"

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	@echo "Installed $(TARGET) to /usr/local/bin/"

# Test targets
test: $(TARGET)
	@echo "Running basic connectivity tests..."
	@echo "Starting server in background..."
	./$(TARGET) &
	@sleep 2
	@echo "Testing HTTP..."
	curl -s http://localhost:8080/test || echo "HTTP test failed"
	@echo "Testing MQTT (requires mosquitto-clients)..."
	timeout 5 mosquitto_pub -h localhost -p 8080 -t test/topic -m "hello" || echo "MQTT test failed (mosquitto-clients not installed?)"
	@echo "Stopping server..."
	pkill -f $(TARGET) || true
	@echo "Tests complete"

# Development helpers
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

release: CFLAGS += -DNDEBUG -O3
release: clean $(TARGET)

# Show help
help:
	@echo "PaumIoT Build System"
	@echo "==================="
	@echo "Targets:"
	@echo "  all      - Build the main executable (default)"
	@echo "  clean    - Remove build files"
	@echo "  debug    - Build with debug symbols"
	@echo "  release  - Build optimized release version"
	@echo "  test     - Run basic connectivity tests"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  help     - Show this help message"

.PHONY: all clean install test debug release help
