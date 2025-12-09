#!/bin/bash

# Download dependencies without git submodules
# This script is for users who download the source code as ZIP instead of git clone

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "============================================"
echo "TGLog SDK - Download Dependencies"
echo "============================================"
echo ""
echo "This script downloads required dependencies:"
echo "  - jemalloc 5.3.0"
echo "  - libevent 2.1.12-stable"
echo "  - ConcurrencyKit 0.7.2"
echo ""

# Check if we're in a git repo
if [ -d ".git" ]; then
    echo "Detected git repository. Using git submodule instead..."
    git submodule update --init --recursive
    echo "✓ Submodules initialized"
    exit 0
fi

echo "Not a git repository. Downloading dependencies manually..."
echo ""

# Function to download and extract
download_and_extract() {
    local name=$1
    local url=$2
    local target_dir=$3
    local version=$4

    if [ -d "$target_dir" ] && [ -n "$(ls -A $target_dir 2>/dev/null)" ]; then
        echo "✓ $name already exists, skipping..."
        return 0
    fi

    echo "Downloading $name $version..."

    # Create temp directory
    local temp_dir=$(mktemp -d)
    local archive="$temp_dir/archive.tar.gz"

    # Download
    if command -v wget >/dev/null 2>&1; then
        wget -q --show-progress -O "$archive" "$url" || {
            echo "✗ Failed to download $name"
            rm -rf "$temp_dir"
            return 1
        }
    elif command -v curl >/dev/null 2>&1; then
        curl -L -o "$archive" "$url" || {
            echo "✗ Failed to download $name"
            rm -rf "$temp_dir"
            return 1
        }
    else
        echo "✗ Neither wget nor curl found. Please install one of them."
        rm -rf "$temp_dir"
        return 1
    fi

    # Extract (auto-detect compression format)
    echo "Extracting $name..."
    mkdir -p "$target_dir"
    tar -xf "$archive" -C "$temp_dir"

    # Move to target directory
    local extracted=$(find "$temp_dir" -mindepth 1 -maxdepth 1 -type d | head -1)
    if [ -n "$extracted" ]; then
        mv "$extracted"/* "$target_dir/"
    fi

    # Cleanup
    rm -rf "$temp_dir"

    echo "✓ $name downloaded and extracted"
    echo ""
}

# Download jemalloc
download_and_extract \
    "jemalloc" \
    "https://github.com/jemalloc/jemalloc/releases/download/5.3.0/jemalloc-5.3.0.tar.bz2" \
    "jemalloc" \
    "5.3.0"

# Download libevent
download_and_extract \
    "libevent" \
    "https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz" \
    "event" \
    "2.1.12-stable"

# Download ConcurrencyKit
download_and_extract \
    "ConcurrencyKit" \
    "https://github.com/concurrencykit/ck/archive/refs/tags/0.7.2.tar.gz" \
    "ck" \
    "0.7.2"

echo "============================================"
echo "✓ All dependencies downloaded successfully!"
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. Run: make deps      # Build dependencies"
echo "  2. Run: make           # Build TGLog SDK"
echo ""
