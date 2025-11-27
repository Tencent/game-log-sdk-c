# Makefile for TgLog Language Bindings (Java/Python)

# Subdirectories
JAVA_DIR = java
PYTHON_DIR = python

# Targets
all: java python

# Java JNI targets - delegate to java subdirectory
java:
	@echo "Building Java JNI bindings..."
	@$(MAKE) -C $(JAVA_DIR) all RELEASE=$(RELEASE)

# Python targets - delegate to python subdirectory
python:
	@echo "Building Python bindings..."
	@$(MAKE) -C $(PYTHON_DIR) all RELEASE=$(RELEASE)

# Test targets
run_java:
	@$(MAKE) -C $(JAVA_DIR) run

run_python:
	@$(MAKE) -C $(PYTHON_DIR) run

# Clean targets
clean: clean_java clean_python

clean_java:
	@echo "Cleaning Java JNI build files..."
	@$(MAKE) -C $(JAVA_DIR) clean

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
	@echo "  all                  - Build all bindings (Java JNI and Python)"
	@echo "  java                 - Build Java/JNI bindings"
	@echo "  python               - Build all Python bindings"
	@echo ""
	@echo "Test targets:"
	@echo "  run_java             - Run Java JNI tests"
	@echo "  run_python           - Run Python tests"
	@echo ""
	@echo "Clean targets:"
	@echo "  clean                - Clean all build files"
	@echo "  clean_java           - Clean Java JNI build files only"
	@echo "  clean_python         - Clean Python build files only"
	@echo "  clean_all            - Deep clean all generated files"
	@echo ""
	@echo "  help                 - Show this help message"
	@echo ""
	@echo "For more options, see:"
	@echo "  make -C java help    - Java JNI-specific targets"
	@echo "  make -C python help  - Python-specific targets"

.PHONY: all java python run_java run_python clean clean_java clean_python clean_all help
