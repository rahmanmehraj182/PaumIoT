#!/bin/bash
# PaumIoT Environment Setup Script
# Sets up development environment for different architectures

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -eq 0 ]]; then
        log_error "This script should not be run as root"
        exit 1
    fi
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [ -f /etc/debian_version ]; then
            OS="debian"
        elif [ -f /etc/redhat-release ]; then
            OS="redhat"
        else
            OS="linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    else
        log_error "Unsupported OS: $OSTYPE"
        exit 1
    fi
    log_info "Detected OS: $OS"
}

# Install dependencies for Debian/Ubuntu
install_debian_deps() {
    log_info "Installing dependencies for Debian/Ubuntu..."
    
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        gcc \
        g++ \
        make \
        git \
        pkg-config \
        libssl-dev \
        doxygen \
        graphviz \
        valgrind \
        cppcheck \
        clang-format
    
    # Optional protocol libraries
    if command -v apt-cache &> /dev/null; then
        if apt-cache search libpaho-mqtt-dev | grep -q libpaho-mqtt-dev; then
            sudo apt-get install -y libpaho-mqtt-dev
        fi
        if apt-cache search libcoap.*-dev | grep -q libcoap; then
            sudo apt-get install -y libcoap2-dev
        fi
        sudo apt-get install -y libmbedtls-dev
    fi
}

# Install dependencies for Red Hat/CentOS/Fedora
install_redhat_deps() {
    log_info "Installing dependencies for Red Hat/CentOS/Fedora..."
    
    if command -v dnf &> /dev/null; then
        PKG_MGR="dnf"
    else
        PKG_MGR="yum"
    fi
    
    sudo $PKG_MGR install -y \
        gcc \
        gcc-c++ \
        make \
        cmake \
        git \
        pkgconfig \
        openssl-devel \
        doxygen \
        graphviz \
        valgrind \
        cppcheck
}

# Install dependencies for macOS
install_macos_deps() {
    log_info "Installing dependencies for macOS..."
    
    if ! command -v brew &> /dev/null; then
        log_error "Homebrew is required but not installed"
        log_info "Install from: https://brew.sh"
        exit 1
    fi
    
    brew install \
        cmake \
        pkg-config \
        doxygen \
        graphviz \
        mosquitto \
        mbedtls
    
    # Install libcoap if available
    brew install libcoap || log_warn "libcoap not available via Homebrew"
}

# Install cross-compilation toolchains
install_cross_toolchains() {
    log_info "Installing cross-compilation toolchains..."
    
    case "$OS" in
        "debian")
            sudo apt-get install -y \
                gcc-arm-linux-gnueabihf \
                g++-arm-linux-gnueabihf \
                gcc-arm-none-eabi
            ;;
        "redhat")
            # Cross-compilation toolchains vary by distribution
            log_warn "Cross-compilation toolchains need manual installation on Red Hat systems"
            ;;
        "macos")
            # ARM toolchains available via Homebrew
            brew install arm-none-eabi-gcc || log_warn "ARM toolchain installation failed"
            ;;
    esac
}

# Setup project directories
setup_directories() {
    log_info "Setting up project directories..."
    
    cd "$PROJECT_ROOT"
    
    # Create necessary directories if they don't exist
    mkdir -p build/{debug,release}
    mkdir -p logs
    mkdir -p tmp
    
    # Set up git hooks if in a git repository
    if [ -d ".git" ]; then
        log_info "Setting up git hooks..."
        if [ -f "scripts/pre-commit" ]; then
            cp scripts/pre-commit .git/hooks/
            chmod +x .git/hooks/pre-commit
        fi
    fi
}

# Initialize build system
init_build_system() {
    log_info "Initializing build system..."
    
    cd "$PROJECT_ROOT"
    
    # Create build directory and run cmake
    mkdir -p build/debug
    cd build/debug
    cmake -DCMAKE_BUILD_TYPE=Debug ../..
    
    cd "$PROJECT_ROOT"
    mkdir -p build/release
    cd build/release
    cmake -DCMAKE_BUILD_TYPE=Release ../..
    
    cd "$PROJECT_ROOT"
    log_info "Build system initialized. Use 'make' or 'cmake --build' to compile."
}

# Setup development environment
setup_dev_env() {
    log_info "Setting up development environment..."
    
    # Create .env file for development
    cat > "$PROJECT_ROOT/.env" << EOF
# PaumIoT Development Environment
export PAUMIOT_ROOT="$PROJECT_ROOT"
export PAUMIOT_BUILD_DIR="$PROJECT_ROOT/build"
export PAUMIOT_CONFIG_DIR="$PROJECT_ROOT/config"
export PAUMIOT_LOG_LEVEL="DEBUG"

# Add project tools to PATH
export PATH="$PROJECT_ROOT/build:$PROJECT_ROOT/scripts:\$PATH"

# Aliases for common tasks
alias paumiot-build='make -C $PROJECT_ROOT'
alias paumiot-test='make -C $PROJECT_ROOT test'
alias paumiot-clean='make -C $PROJECT_ROOT clean'
alias paumiot-docs='make -C $PROJECT_ROOT docs'
EOF
    
    log_info "Development environment configured."
    log_info "Source the environment with: source $PROJECT_ROOT/.env"
}

# Main setup function
main() {
    log_info "Starting PaumIoT environment setup..."
    
    check_root
    detect_os
    
    # Install dependencies based on OS
    case "$OS" in
        "debian")
            install_debian_deps
            ;;
        "redhat")
            install_redhat_deps
            ;;
        "macos")
            install_macos_deps
            ;;
    esac
    
    # Setup project
    setup_directories
    init_build_system
    setup_dev_env
    
    # Optional: Install cross-compilation toolchains
    read -p "Install cross-compilation toolchains? [y/N]: " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        install_cross_toolchains
    fi
    
    log_info "Setup complete!"
    log_info "Next steps:"
    log_info "  1. Source the environment: source .env"
    log_info "  2. Build the project: make"
    log_info "  3. Run tests: make test"
    log_info "  4. Generate docs: make docs"
}

# Run main function
main "$@"