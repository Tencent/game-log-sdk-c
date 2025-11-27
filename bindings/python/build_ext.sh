#!/bin/bash
# Build script for tglog C extension

echo "Building tglog C extension..."

# Clean previous builds
rm -rf build/
rm -f tglog_ext*.so

# Build the extension
python3 setup_ext.py build_ext --inplace

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo "The extension module tglog_ext*.so has been created in the current directory."
    echo ""
    echo "To test the extension, run:"
    echo "  python3 -c 'import tglog_ext; print(\"✓ Import successful\")'"
    echo "  python3 test_ext.py -t 10000"
else
    echo "✗ Build failed!"
    exit 1
fi