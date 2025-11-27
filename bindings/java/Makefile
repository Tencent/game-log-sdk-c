# Makefile for TgLog Java/JNI Bindings

# Java and JNI settings
JAVA_HOME ?= $(shell readlink -f /usr/bin/javac | sed "s:bin/javac::")
JNI_INCLUDES = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -I$(JAVA_HOME)/include/darwin

# Compiler settings
CC = gcc
CFLAGS = -Wall -fPIC -shared -pthread -std=c99
JAVA_CFLAGS = $(JNI_INCLUDES) -I../../tglog -I../../ck/include -I../../event/include -I../../jemalloc/include

# Build optimization
ifdef RELEASE
    CFLAGS += -O2 -DNDEBUG
else
    CFLAGS += -g -O0 -DDEBUG
endif

# Library paths
TGLOG_LIB = ../../lib/libtglog.so
JNI_LIB = libtglog_jni.so
LIB_DIR = $(shell cd ../../lib && pwd)

# Source files
JNI_SOURCE = TglogJNI.c
JAVA_SOURCES = TglogJNI.java TglogConfig.java TglogCallback.java TglogException.java \
               TglogStats.java TglogClient.java TglogTest.java TglogClientTest.java \
               TglogLogLevel.java TglogCanLogCallback.java TglogWriteLogCallback.java
JAVA_CLASSES = $(JAVA_SOURCES:.java=.class)

# Targets
all: compile_java compile_jni

compile_java: $(JAVA_CLASSES)

%.class: %.java
	@echo "Compiling $<..."
	javac -cp . $<

compile_jni:
	@echo "Compiling JNI library..."
	$(CC) $(CFLAGS) $(JAVA_CFLAGS) -o $(JNI_LIB) $(JNI_SOURCE) -L../../lib -ltglog -Wl,-rpath,$(LIB_DIR)

# Install target
install: all
	@echo "Installing Java bindings..."
	@mkdir -p ../../dist/bindings/java
	@cp -f $(JNI_LIB) ../../dist/bindings/java/
	@cp -f *.java ../../dist/bindings/java/ 2>/dev/null || true
	@cp -f *.class ../../dist/bindings/java/ 2>/dev/null || true
	@cp -f $(JNI_SOURCE) ../../dist/bindings/java/ 2>/dev/null || true
	@cp -f TglogJNI.h ../../dist/bindings/java/ 2>/dev/null || true
	@cp -f README_JAVA.md ../../dist/bindings/java/ 2>/dev/null || true

# Test targets
run: all
	@echo "Running Java TglogTest..."
	@echo "Library path: $(LIB_DIR)"
	DYLD_LIBRARY_PATH=$(LIB_DIR):$$DYLD_LIBRARY_PATH LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH java -Djava.library.path=. -cp . TglogTest

run_with_args: all
	@echo "Running Java TglogTest with arguments..."
	@echo "Library path: $(LIB_DIR)"
	DYLD_LIBRARY_PATH=$(LIB_DIR):$$DYLD_LIBRARY_PATH LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH java -Djava.library.path=. -cp . TglogTest -t 1000 -r 100

run_client_test: all
	@echo "Running TglogClient performance test..."
	@echo "Library path: $(LIB_DIR)"
	DYLD_LIBRARY_PATH=$(LIB_DIR):$$DYLD_LIBRARY_PATH LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH java -Djava.library.path=. -cp . TglogClientTest

# Clean targets
clean:
	@echo "Cleaning Java build files..."
	rm -f *.class $(JNI_LIB)

# Help
help:
	@echo "TgLog Java/JNI Bindings Makefile"
	@echo "================================="
	@echo ""
	@echo "Targets:"
	@echo "  all                  - Build Java classes and JNI library"
	@echo "  compile_java         - Compile Java sources only"
	@echo "  compile_jni          - Compile JNI library only"
	@echo "  install              - Install to dist/bindings/java"
	@echo "  run                  - Run Java performance test (TglogTest)"
	@echo "  run_with_args        - Run Java test with sample arguments"
	@echo "  run_client_test      - Run TglogClient performance test"
	@echo "  clean                - Clean build files"
	@echo "  help                 - Show this help message"

.PHONY: all compile_java compile_jni install run run_with_args run_client_test clean help
