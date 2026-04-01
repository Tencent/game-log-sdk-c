#!/usr/bin/env python3
"""
Setup script for building Cython extension
Run: python setup_cython.py build_ext --inplace
"""

from setuptools import setup, Extension
from Cython.Build import cythonize
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

# Define the Cython extension module
extensions = [
    Extension(
        "tglog_cython",
        sources=["tglog_cython.pyx"],
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
        language='c',
        define_macros=[('_GNU_SOURCE', None)],
    )
]

setup(
    name='tglog_cython',
    version='1.0.0',
    description='High-performance Cython extension for TgLog',
    ext_modules=cythonize(
        extensions,
        compiler_directives={
            'language_level': 3,
            'boundscheck': False,
            'wraparound': False,
            'cdivision': True,
            'nonecheck': False,
        }
    ),
    zip_safe=False,
    python_requires='>=3.6',
)