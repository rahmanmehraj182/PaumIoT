# Multi-stage Docker build for PaumIoT
# Supports both development and production environments

# Development stage with full toolchain
FROM ubuntu:20.04 as development

# Prevent timezone prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install development tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gcc \
    g++ \
    make \
    git \
    pkg-config \
    libssl-dev \
    libpaho-mqtt-dev \
    libcoap2-dev \
    libmbedtls-dev \
    doxygen \
    graphviz \
    valgrind \
    cppcheck \
    clang-format \
    cpplint \
    && rm -rf /var/lib/apt/lists/*

# Cross-compilation toolchains
RUN apt-get update && apt-get install -y \
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    gcc-arm-none-eabi \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . .

# Set up development environment
RUN chmod +x scripts/*.sh

# Production build stage
FROM development as builder

# Build the application
RUN make clean && make release

# Production runtime stage
FROM ubuntu:20.04 as production

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libpaho-mqtt1.3 \
    libcoap2 \
    libmbedtls12 \
    && rm -rf /var/lib/apt/lists/*

# Create application user
RUN useradd -r -s /bin/false paumiot

# Copy built application
COPY --from=builder /workspace/build/paumiot /usr/local/bin/
COPY --from=builder /workspace/config /etc/paumiot/

# Create necessary directories
RUN mkdir -p /var/log/paumiot /var/lib/paumiot && \
    chown -R paumiot:paumiot /var/log/paumiot /var/lib/paumiot

# Expose ports
EXPOSE 1883 8883 5683 5684 9090

# Switch to application user
USER paumiot

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:9090/health || exit 1

# Default command
CMD ["/usr/local/bin/paumiot", "--config", "/etc/paumiot/paumiot.conf"]