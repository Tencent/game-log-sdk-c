#!/usr/bin/env python3
"""
Setup script for building C extension
Run: python setup_ext.py build_ext --inplace
"""

from distutils.core import setup, Extension
import os
import sys
import platform

# Check platform - we only support Linux/Unix
if platform.system() == "Windows":
    print("Error: tglog is not supported on Windows platform.", file=sys.stderr)
    print("Please use Linux or macOS.", file=sys.stderr)
    sys.exit(1)

# Get the directory paths
current_dir = os.path.dirname(os.path.abspath(__file__))
# Go up two levels: python -> bindings -> sdk-c
sdk_root = os.path.dirname(os.path.dirname(current_dir))

# Define the extension module
tglog_ext = Extension(
    'tglog_ext',
    sources=['tglog_ext.c'],
    include_dirs=[
        os.path.join(sdk_root, 'tglog'),
        os.path.join(sdk_root, 'thirdparty/ck/include'),
        os.path.join(sdk_root, 'thirdparty/event/include'),
    ],
    library_dirs=[
        os.path.join(sdk_root, 'lib'),
        os.path.join(sdk_root, 'thirdparty/event/lib'),
        os.path.join(sdk_root, 'thirdparty/ck/lib'),
    ],
    libraries=['tglog', 'event', 'ck', 'pthread'],
    extra_compile_args=['-O3', '-march=native', '-ffast-math', '-DNDEBUG', '-std=c99'],
    extra_link_args=[
        '-Wl,-rpath,' + os.path.join(sdk_root, 'lib'),
        '-Wl,-rpath,' + os.path.join(sdk_root, 'thirdparty/event/lib'),
        '-Wl,-rpath,' + os.path.join(sdk_root, 'thirdparty/ck/lib'),
    ],
)

setup(
    name='tglog_ext',
    version='1.0.0',
    description='High-performance Python extension for TgLog',
    ext_modules=[tglog_ext],
)