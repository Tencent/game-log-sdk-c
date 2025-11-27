# Makefile for TgLog Python Bindings

# Python settings
PYTHON ?= python3
PIP ?= pip3

# Library paths
TGLOG_LIB = ../../lib/libtglog.so
LIB_DIR = $(shell cd ../../lib && pwd)

# Python extension files
PYTHON_CTYPES = tglog_client.py
PYTHON_EXT_SOURCE = tglog_ext.c
PYTHON_CYTHON_SOURCE = tglog_cython.pyx

# Targets
all: python_check python_ctypes python_ext python_cython

python_check:
	@echo "Checking Python dependencies..."
	@$(PYTHON) -c "import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)" || \
		(echo "Error: Python 3.6+ is required" && exit 1)

python_ctypes:
	@echo "Python ctypes binding (tglog_client.py) is ready to use"
	@echo "  No compilation needed for ctypes - uses libtglog.so directly"

python_ext: $(TGLOG_LIB)
	@echo "Building Python C extension..."
	@if [ -f setup_ext.py ]; then \
		$(PYTHON) setup_ext.py build_ext --inplace; \
	else \
		echo "Warning: setup_ext.py not found, skipping C extension build"; \
	fi

python_cython: $(TGLOG_LIB)
	@echo "Building Cython extension..."
	@if $(PYTHON) -c "import Cython" 2>/dev/null; then \
		if [ -f setup_cython.py ]; then \
			$(PYTHON) setup_cython.py build_ext --inplace; \
		else \
			echo "Warning: setup_cython.py not found, skipping Cython build"; \
		fi \
	else \
		echo "Warning: Cython not installed. Install with: $(PIP) install cython"; \
		echo "Skipping Cython extension build"; \
	fi

# Install Python dependencies
python_deps:
	@echo "Installing Python dependencies..."
	$(PIP) install --user cython numpy

# Install target
install: all
	@echo "Installing Python bindings..."
	@mkdir -p ../../dist/bindings/python
	@cp -f *.py ../../dist/bindings/python/ 2>/dev/null || true
	@cp -f *.pyx ../../dist/bindings/python/ 2>/dev/null || true
	@cp -f tglog_ext.c ../../dist/bindings/python/ 2>/dev/null || true
	@cp -f tglog_cython.c ../../dist/bindings/python/ 2>/dev/null || true
	@cp -f README_PYTHON.md ../../dist/bindings/python/ 2>/dev/null || true

# Test targets
run_ctypes: python_ctypes
	@echo "Running Python ctypes test..."
	@echo "Library path: $(LIB_DIR)"
	DYLD_LIBRARY_PATH=$(LIB_DIR):$$DYLD_LIBRARY_PATH LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH $(PYTHON) tglog_test.py

run_ext: python_ext
	@echo "Running Python C extension test..."
	@echo "Library path: $(LIB_DIR)"
	@if [ -f tglog_ext*.so ]; then \
		DYLD_LIBRARY_PATH=$(LIB_DIR):$$DYLD_LIBRARY_PATH LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH $(PYTHON) test_ext.py; \
	else \
		echo "Error: Python C extension not built. Run 'make python_ext' first"; \
	fi

run_cython: python_cython
	@echo "Running Cython extension test..."
	@echo "Library path: $(LIB_DIR)"
	@if [ -f tglog_cython*.so ]; then \
		DYLD_LIBRARY_PATH=$(LIB_DIR):$$DYLD_LIBRARY_PATH LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH $(PYTHON) test_cython.py; \
	else \
		echo "Error: Cython extension not built. Run 'make python_cython' first"; \
	fi

run: run_ctypes run_ext run_cython
	@echo "All Python tests completed"

# Clean targets
clean:
	@echo "Cleaning Python build files..."
	rm -rf build/ dist/ *.egg-info/
	rm -f tglog_ext*.so tglog_cython*.so
	rm -f tglog_cython.c
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true

# Help
help:
	@echo "TgLog Python Bindings Makefile"
	@echo "==============================="
	@echo ""
	@echo "Targets:"
	@echo "  all                  - Build all Python bindings"
	@echo "  python_deps          - Install Python dependencies (cython, numpy)"
	@echo "  python_ctypes        - Check ctypes binding (no compilation needed)"
	@echo "  python_ext           - Build Python C extension"
	@echo "  python_cython        - Build Cython extension"
	@echo "  install              - Install to dist/bindings/python"
	@echo "  run_ctypes           - Run ctypes binding test"
	@echo "  run_ext              - Run C extension test"
	@echo "  run_cython           - Run Cython extension test"
	@echo "  run                  - Run all Python tests"
	@echo "  clean                - Clean build files"
	@echo "  help                 - Show this help message"

.PHONY: all python_check python_ctypes python_ext python_cython python_deps \
        install run_ctypes run_ext run_cython run clean help
