#!/bin/bash
# Test script to verify submodule initialization

echo "==================================="
echo "Testing Submodule Initialization"
echo "==================================="
echo ""

# Check if we're in the right directory
if [ ! -f "Makefile" ]; then
    echo "Error: Must run from project root directory"
    exit 1
fi

echo "1. Checking current submodule versions..."
echo ""

if [ -d "jemalloc/.git" ]; then
    JEMALLOC_VER=$(cd jemalloc && git describe --tags 2>/dev/null || echo "unknown")
    echo "  jemalloc: $JEMALLOC_VER"
else
    echo "  jemalloc: NOT INITIALIZED"
fi

if [ -d "event/.git" ]; then
    EVENT_VER=$(cd event && git describe --tags 2>/dev/null || echo "unknown")
    echo "  libevent: $EVENT_VER"
else
    echo "  libevent: NOT INITIALIZED"
fi

if [ -d "ck/.git" ]; then
    CK_VER=$(cd ck && git describe --tags 2>/dev/null || echo "unknown")
    echo "  ck:       $CK_VER"
else
    echo "  ck:       NOT INITIALIZED"
fi

echo ""
echo "2. Expected versions:"
echo "  jemalloc: 5.3.0"
echo "  libevent: release-2.1.12-stable"
echo "  ck:       0.7.2"

echo ""
echo "3. Running 'make init-submodules'..."
echo ""

make init-submodules

echo ""
echo "4. Verifying versions after initialization..."
echo ""

JEMALLOC_VER=$(cd jemalloc && git describe --tags 2>/dev/null || echo "unknown")
EVENT_VER=$(cd event && git describe --tags 2>/dev/null || echo "unknown")
CK_VER=$(cd ck && git describe --tags 2>/dev/null || echo "unknown")

echo "  jemalloc: $JEMALLOC_VER"
echo "  libevent: $EVENT_VER"
echo "  ck:       $CK_VER"

echo ""
echo "5. Verification:"

SUCCESS=true

if [ "$JEMALLOC_VER" != "5.3.0" ]; then
    echo "  ✗ jemalloc version mismatch"
    SUCCESS=false
else
    echo "  ✓ jemalloc version correct"
fi

if [ "$EVENT_VER" != "release-2.1.12-stable" ]; then
    echo "  ✗ libevent version mismatch"
    SUCCESS=false
else
    echo "  ✓ libevent version correct"
fi

if [ "$CK_VER" != "0.7.2" ]; then
    echo "  ✗ ck version mismatch"
    SUCCESS=false
else
    echo "  ✓ ck version correct"
fi

echo ""
if [ "$SUCCESS" = true ]; then
    echo "✓ All submodules are at the correct versions!"
    exit 0
else
    echo "✗ Some submodules are not at the correct versions"
    exit 1
fi
