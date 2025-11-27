#!/bin/bash
# TgLog Java Test Runner
#
# This script sets up the environment and runs the Java test program.
# Note: JNI library does not link jemalloc - TGLog library uses its own
# memory pools and object pools for optimization.

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Set library paths
export LD_LIBRARY_PATH="$SCRIPT_DIR:$SDK_ROOT/lib:$SDK_ROOT/thirdparty/event/lib:$SDK_ROOT/thirdparty/ck/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$SCRIPT_DIR:$SDK_ROOT/lib:$SDK_ROOT/thirdparty/event/lib:$SDK_ROOT/thirdparty/ck/lib:$DYLD_LIBRARY_PATH"

echo "Library paths configured"

# Run Java with JNI library path
exec java -Djava.library.path="$SCRIPT_DIR" -cp "$SCRIPT_DIR" TglogTest "$@"
