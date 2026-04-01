#!/usr/bin/env python3
"""
Setup script for building C extension
Run: python setup_ext.py build_ext --inplace
"""

from distutils.core import setup, Extension
import os
import sys

# Get the directory paths
current_dir = os.path.dirname(os.path.abspath(__file__))
# Go up two levels: python -> bindings -> sdk-c
sdk_root = os.path.dirname(os.path.dirname(current_dir))

# Platform-aware build configuration
if sys.platform == 'win32':
    libraries = ['tglog', 'ws2_32']
    extra_compile_args = ['/O2', '/DNDEBUG']
    extra_link_args = []
elif sys.platform == 'darwin':
    libraries = ['tglog', 'event', 'pthread']
    extra_compile_args = ['-O3', '-march=native', '-ffast-math', '-DNDEBUG', '-std=c99']
    extra_link_args = [
        '-Wl,-rpath,' + os.path.join(sdk_root, 'lib'),
        '-Wl,-rpath,' + os.path.join(sdk_root, 'thirdparty/event/lib'),
    ]
else:
    libraries = ['tglog', 'event', 'pthread']
    extra_compile_args = ['-O3', '-march=native', '-ffast-math', '-DNDEBUG', '-std=c99']
    extra_link_args = [
        '-Wl,-rpath,' + os.path.join(sdk_root, 'lib'),
        '-Wl,-rpath,' + os.path.join(sdk_root, 'thirdparty/event/lib'),
    ]

# Define the extension module
tglog_ext = Extension(
    'tglog_ext',
    sources=['tglog_ext.c'],
    include_dirs=[
        os.path.join(sdk_root, 'tglog'),
        os.path.join(sdk_root, 'common'),
        os.path.join(sdk_root, 'thirdparty/event/include'),
        os.path.join(sdk_root, 'event/include'),
    ],
    library_dirs=[
        os.path.join(sdk_root, 'lib'),
        os.path.join(sdk_root, 'thirdparty/event/lib'),
    ],
    libraries=libraries,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
)

setup(
    name='tglog_ext',
    version='1.0.0',
    description='High-performance Python extension for TgLog',
    ext_modules=[tglog_ext],
)