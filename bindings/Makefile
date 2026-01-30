# Makefile for TgLog Language Bindings (Python)

# Subdirectories
PYTHON_DIR = python

# Targets
all: python

# Python targets - delegate to python subdirectory
python:
	@echo "Building Python bindings..."
	@$(MAKE) -C $(PYTHON_DIR) all RELEASE=$(RELEASE)

# Test targets
run_python:
	@$(MAKE) -C $(PYTHON_DIR) run

# Clean targets
clean: clean_python

clean_python:
	@echo "Cleaning Python build files..."
	@$(MAKE) -C $(PYTHON_DIR) clean

clean_all: clean
	@echo "Deep cleaning all generated files..."

# Help
help:
	@echo "TgLog Language Bindings Makefile"
	@echo "================================="
	@echo ""
	@echo "Main targets:"
	@echo "  all                  - Build all bindings (Python)"
	@echo "  python               - Build all Python bindings"
	@echo ""
	@echo "Test targets:"
	@echo "  run_python           - Run Python tests"
	@echo ""
	@echo "Clean targets:"
	@echo "  clean                - Clean all build files"
	@echo "  clean_python         - Clean Python build files only"
	@echo "  clean_all            - Deep clean all generated files"
	@echo ""
	@echo "  help                 - Show this help message"
	@echo ""
	@echo "For more options, see:"
	@echo "  make -C python help  - Python-specific targets"

.PHONY: all python run_python clean clean_python clean_all help
