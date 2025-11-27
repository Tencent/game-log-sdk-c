#!/bin/bash
# Build script for tglog Cython extension

echo "Building tglog Cython extension..."

# Check if Cython is installed
if ! python3 -c "import Cython" 2>/dev/null; then
    echo "Error: Cython is not installed!"
    echo "Please install Cython first:"
    echo "  pip3 install Cython"
    exit 1
fi

# Clean previous builds
rm -rf build/
rm -f tglog_cython*.so
rm -f tglog_cython.c

# Build the extension
python3 setup_cython.py build_ext --inplace

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo "The extension module tglog_cython*.so has been created in the current directory."
    echo ""
    echo "To test the extension, run:"
    echo "  python3 -c 'import tglog_cython; print(\"✓ Import successful\")'"
    echo "  python3 test_cython.py -t 10000"
else
    echo "✗ Build failed!"
    exit 1
fi
