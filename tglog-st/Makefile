# Makefile for TGLog single-threaded version

# Compiler and tools
CC = gcc
AR = ar
RM = rm -f

# Directories
LIB_DIR = ../lib
THIRD_PARTY_DIR = ../thirdparty
EVENT_DIR = $(THIRD_PARTY_DIR)/event
JEMALLOC_DIR = $(THIRD_PARTY_DIR)/jemalloc
COMMON_DIR = ..

# Target libraries
LIB_STATIC = $(LIB_DIR)/libtglog-st.a
LIB_SHARED = $(LIB_DIR)/libtglog-st.so

# Compiler flags
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -fPIC
CFLAGS += -D_REENTRANT

# Build optimization
ifdef RELEASE
    CFLAGS += -O2 -DNDEBUG
else
    CFLAGS += -g -O0 -DDEBUG
endif

# Include paths
INCLUDES = -I. \
           -I$(COMMON_DIR)/common \
           -I$(EVENT_DIR)/include \
           -I$(JEMALLOC_DIR)/include

# Library paths
LDFLAGS = -L$(EVENT_DIR)/lib \
          -L$(JEMALLOC_DIR)/lib

# Static libraries to link (jemalloc should be linked by final executable, not by the library)
STATIC_LIBS = event

# Dynamic libraries (platform-specific)
ifeq ($(OS),Windows_NT)
    DYNAMIC_LIBS = ws2_32
else ifeq ($(shell uname),Darwin)
    DYNAMIC_LIBS = pthread m
else
    DYNAMIC_LIBS = pthread m rt
endif

# Source files (explicitly listed for single-threaded version)
SRCS = client.c \
       conn.c \
       endpoint.c \
       request.c \
       pool.c \
       conf.c \
       err.c \
       metrics.c \
       debug_logger.c \
       worker.c \
       discoverer.c \
       $(COMMON_DIR)/common/tglog_ht.c \
       $(COMMON_DIR)/common/tglog_platform.c

# Object files
OBJS = $(SRCS:.c=.o)

# Dependency files
DEPS = $(OBJS:.o=.d)

# Header files (for reference)
HEADERS = client.h \
          conn.h \
          endpoint.h \
          request.h \
          pool.h \
          conf.h \
          err.h \
          def.h \
          metrics.h \
          debug_logger.h \
          worker.h \
          discoverer.h \
          tglog.h

# Missing dependency files
MISSING_DEPS = $(filter-out $(wildcard $(DEPS)),$(DEPS))
MISSING_DEPS_SOURCES = $(wildcard $(patsubst %.d,%.c,$(MISSING_DEPS)))

# Default target
.PHONY: all
all: $(LIB_STATIC)
ifdef BUILD_SHARED
all: $(LIB_STATIC) $(LIB_SHARED)
endif

# Create lib directory
$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

# Static library
$(LIB_STATIC): $(OBJS) | $(LIB_DIR)
	@echo "Building static library: $@"
	@$(AR) rcs $@ $^

# Shared library
$(LIB_SHARED): $(OBJS) | $(LIB_DIR)
	@echo "Building shared library: $@"
ifeq ($(shell uname),Darwin)
	@$(CC) -shared -o $@ $^ $(LDFLAGS) \
		$(addprefix -l,$(STATIC_LIBS)) \
		$(addprefix -l,$(DYNAMIC_LIBS))
else
	@$(CC) -shared -o $@ $^ $(LDFLAGS) \
		-Wl,-Bstatic $(addprefix -l,$(STATIC_LIBS)) \
		-Wl,-Bdynamic $(addprefix -l,$(DYNAMIC_LIBS))
endif

# Verbose build
ifdef V
    Q =
else
    Q = @
endif

# Compile source files with automatic dependency generation
%.o: %.c
	@echo "Compiling: $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@

# Include dependency files
-include $(DEPS)

# Handle missing dependencies
ifneq ($(MISSING_DEPS),)
$(MISSING_DEPS):
	@$(RM) $(patsubst %.d,%.o,$@)
endif

# Clean targets
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@$(RM) $(OBJS) $(DEPS)
	@$(RM) *.o *.d
	@$(RM) $(LIB_STATIC) $(LIB_SHARED)

.PHONY: rebuild
rebuild: clean all

# Install target
.PHONY: install
install: all
	@echo "Installing TGLog-ST library..."
	@mkdir -p ../dist/lib
	@cp -f $(LIB_STATIC) ../dist/lib/
ifdef BUILD_SHARED
	@if [ -f "$(LIB_SHARED)" ]; then \
		cp -f $(LIB_SHARED) ../dist/lib/; \
	fi
endif
	@mkdir -p ../dist/include/tglog-st
	@cp -f $(HEADERS) ../dist/include/tglog-st/
	@mkdir -p ../dist/include/tglog-st/common
	@cp -f ../common/*.h ../dist/include/tglog-st/common/
	@echo "Installation complete!"

# Debug information
.PHONY: info
info:
	@echo "Source files: $(SRCS)"
	@echo "Object files: $(OBJS)"
	@echo "Header files: $(HEADERS)"
	@echo "Dependency files: $(DEPS)"
	@echo "Missing deps: $(MISSING_DEPS)"
	@echo "Missing sources: $(MISSING_DEPS_SOURCES)"

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all       - Build libraries (default)"
	@echo "  clean     - Remove all generated files"
	@echo "  rebuild   - Clean and rebuild everything"
	@echo "  install   - Install libraries and headers"
	@echo "  info      - Display build information"
	@echo "  help      - Display this help message"
	@echo ""
	@echo "Build options:"
	@echo "  BUILD_SHARED=1  - Also build shared library"