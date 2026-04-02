#!/usr/bin/env python3
"""
Setup script for building C extension
Run: python setup_ext.py build_ext --inplace
"""

try:
    from setuptools import setup, Extension
except ImportError:
    from distutils.core import setup, Extension
import os
import sys

# Get the directory paths
current_dir = os.path.dirname(os.path.abspath(__file__))
# Go up two levels: python -> bindings -> sdk-c
sdk_root = os.path.dirname(os.path.dirname(current_dir))

# Platform-aware build configuration
if sys.platform == 'win32':
    # MSVC outputs libraries to lib/Release/ subdirectory
    import glob as _glob
    # Search for tglog.lib in lib/ and lib/Release/
    lib_dir_candidates = [
        os.path.join(sdk_root, 'lib', 'Release'),
        os.path.join(sdk_root, 'lib'),
    ]
    # Also search for libevent in out/event_build/lib/Release
    # Try common build output directories
    event_lib_candidates = [
        os.path.join(sdk_root, 'out', 'event_build', 'lib', 'Release'),
        os.path.join(sdk_root, 'build', 'event_build', 'lib', 'Release'),
        os.path.join(sdk_root, 'thirdparty', 'event', 'lib'),
    ]
    event_lib_dir = ''
    for d in event_lib_candidates:
        if os.path.isdir(d):
            event_lib_dir = d
            break
    library_dirs_win = [d for d in lib_dir_candidates if os.path.isdir(d)]
    if event_lib_dir:
        library_dirs_win.append(event_lib_dir)
    libraries = ['tglog', 'event_core', 'event_extra', 'ws2_32', 'iphlpapi', 'advapi32', 'shell32']
    extra_compile_args = ['/O2', '/DNDEBUG', '/utf-8']
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
# Resolve include dirs for libevent (event_build for Windows CMake build)
event_include_candidates = [
    os.path.join(sdk_root, 'out', 'event_build', 'include'),
    os.path.join(sdk_root, 'build', 'event_build', 'include'),
    os.path.join(sdk_root, 'thirdparty', 'event', 'include'),
    os.path.join(sdk_root, 'event', 'include'),
]
event_include_dir = os.path.join(sdk_root, 'thirdparty', 'event', 'include')
for d in event_include_candidates:
    if os.path.isdir(d):
        event_include_dir = d
        break

if sys.platform == 'win32':
    _library_dirs = library_dirs_win
else:
    _library_dirs = [
        os.path.join(sdk_root, 'lib'),
        os.path.join(sdk_root, 'thirdparty/event/lib'),
    ]

tglog_ext = Extension(
    'tglog_ext',
    sources=['tglog_ext.c'],
    include_dirs=[
        os.path.join(sdk_root, 'tglog'),
        os.path.join(sdk_root, 'common'),
        event_include_dir,
        os.path.join(sdk_root, 'event', 'include'),
    ],
    library_dirs=_library_dirs,
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