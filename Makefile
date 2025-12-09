# TGLog SDK Makefile
# ==================

# Directories
CURRENT_DIR = $(shell pwd)
JEMALLOC_SRC_DIR = $(CURRENT_DIR)/jemalloc
JEMALLOC_INSTALL_DIR = $(CURRENT_DIR)/thirdparty/jemalloc
JEMALLOC_LIB = $(JEMALLOC_INSTALL_DIR)/lib/libjemalloc.a
JEMALLOC_VERSION = 5.3.0

EVENT_SRC_DIR = $(CURRENT_DIR)/event
EVENT_INSTALL_DIR = $(CURRENT_DIR)/thirdparty/event
EVENT_LIB = $(EVENT_INSTALL_DIR)/lib/libevent.a
EVENT_VERSION = release-2.1.12-stable

CK_SRC_DIR = $(CURRENT_DIR)/ck
CK_INSTALL_DIR = $(CURRENT_DIR)/thirdparty/ck
CK_LIB = $(CK_INSTALL_DIR)/lib/libck.a
CK_VERSION = 0.7.2

LIB_DIR = $(CURRENT_DIR)/lib

# Compiler settings
CC = gcc
CFLAGS = -std=c99 -D_GNU_SOURCE -D_REENTRANT -Wno-typedef-redefinition -I$(EVENT_INSTALL_DIR)/include -I$(CK_INSTALL_DIR)/include -I.
LDFLAGS = -L$(EVENT_INSTALL_DIR)/lib -L$(CK_INSTALL_DIR)/lib -levent -lck -lpthread

# Build optimization
ifdef RELEASE
    CFLAGS += -O2 -DNDEBUG
else
    CFLAGS += -g -O0 -DDEBUG
endif

# Verbose build
ifdef V
    Q =
else
    Q = @
endif

# Targets
SUB_DIRS = tglog tglog-st test test-st test-tapp bindings
DEPS = $(JEMALLOC_LIB) $(EVENT_LIB) $(CK_LIB)

.PHONY: all deps tglog tglog-st test test-tapp examples bindings java python clean clean-deps clean-bindings uninstall check-deps help install init-submodules

# Main targets
all: deps tglog tglog-st test test-tapp bindings
	$(Q)echo "✓ Build completed successfully!"

# Initialize and update git submodules
init-submodules:
	$(Q)echo "Checking dependencies..."
	$(Q)if [ ! -d "$(JEMALLOC_SRC_DIR)" ] || [ ! "$(shell ls -A $(JEMALLOC_SRC_DIR) 2>/dev/null)" ] || \
	   [ ! -d "$(EVENT_SRC_DIR)" ] || [ ! "$(shell ls -A $(EVENT_SRC_DIR) 2>/dev/null)" ] || \
	   [ ! -d "$(CK_SRC_DIR)" ] || [ ! "$(shell ls -A $(CK_SRC_DIR) 2>/dev/null)" ]; then \
		echo "  Dependencies not found, initializing..."; \
		if [ -d ".git" ]; then \
			echo "  Using git submodules..."; \
			git submodule update --init --recursive || { \
				echo ""; \
				echo "✗ Failed to initialize submodules"; \
				exit 1; \
			}; \
		else \
			echo "  Running download-deps.sh to download dependencies..."; \
			./download-deps.sh || { \
				echo ""; \
				echo "✗ Failed to download dependencies"; \
				exit 1; \
			}; \
		fi; \
	else \
		echo "  ✓ Dependencies found"; \
	fi
	$(Q)if [ -d ".git" ]; then \
		echo "  Checking out specific versions..."; \
		if [ -e "$(JEMALLOC_SRC_DIR)/.git" ]; then \
			cd $(JEMALLOC_SRC_DIR) && \
			CURRENT=$$(git describe --tags --exact-match 2>/dev/null || git rev-parse --short HEAD 2>/dev/null || echo "unknown"); \
			if [ "$$CURRENT" != "$(JEMALLOC_VERSION)" ]; then \
				echo "    jemalloc: $$CURRENT -> $(JEMALLOC_VERSION)"; \
				git checkout $(JEMALLOC_VERSION) 2>/dev/null || true; \
			fi; \
		fi; \
		if [ -e "$(EVENT_SRC_DIR)/.git" ]; then \
			cd $(EVENT_SRC_DIR) && \
			CURRENT=$$(git describe --tags --exact-match 2>/dev/null || git rev-parse --short HEAD 2>/dev/null || echo "unknown"); \
			if [ "$$CURRENT" != "$(EVENT_VERSION)" ]; then \
				echo "    libevent: $$CURRENT -> $(EVENT_VERSION)"; \
				git checkout $(EVENT_VERSION) 2>/dev/null || true; \
			fi; \
		fi; \
		if [ -e "$(CK_SRC_DIR)/.git" ]; then \
			cd $(CK_SRC_DIR) && \
			CURRENT=$$(git describe --tags --exact-match 2>/dev/null || git rev-parse --short HEAD 2>/dev/null || echo "unknown"); \
			if [ "$$CURRENT" != "$(CK_VERSION)" ]; then \
				echo "    ck: $$CURRENT -> $(CK_VERSION)"; \
				git checkout $(CK_VERSION) 2>/dev/null || true; \
			fi; \
		fi; \
		echo "✓ Submodules checked out to specified versions"; \
	else \
		echo "  ✓ Using downloaded dependencies (version check skipped)"; \
	fi

# Build only dependencies
deps: init-submodules $(DEPS)
	$(Q)echo "✓ Dependencies built successfully!"

# Build only TGLog library (both static and shared)
tglog: $(DEPS)
	$(Q)echo "Building TGLog library (static and shared)..."
	$(Q)$(MAKE) -C tglog BUILD_SHARED=1 RELEASE=$(RELEASE)

# Build TGLog single-threaded library
tglog-st: $(DEPS)
	$(Q)echo "Building TGLog single-threaded library..."
	$(Q)$(MAKE) -C tglog-st RELEASE=$(RELEASE)

# Build test programs (examples)
test: tglog tglog-st
	$(Q)echo "Building test programs..."
	$(Q)$(MAKE) -C test RELEASE=$(RELEASE)
	$(Q)$(MAKE) -C test-st RELEASE=$(RELEASE)

# Build test-tapp program
test-tapp: tglog-st
	$(Q)echo "Building test-tapp program..."
	$(Q)$(MAKE) -C test-tapp RELEASE=$(RELEASE)

examples: test

# Build language bindings
bindings: tglog
	$(Q)echo "Building language bindings..."
	$(Q)$(MAKE) -C bindings all RELEASE=$(RELEASE)

# Build only Java bindings
java: tglog
	$(Q)echo "Building Java bindings..."
	$(Q)$(MAKE) -C bindings java RELEASE=$(RELEASE)

# Build only Python bindings
python: tglog
	$(Q)echo "Building Python bindings..."
	$(Q)$(MAKE) -C bindings python RELEASE=$(RELEASE)

# Dependency build rules
$(JEMALLOC_LIB):
	$(Q)echo "Building jemalloc..."
	$(Q)mkdir -p $(JEMALLOC_INSTALL_DIR)
	$(Q)cd $(JEMALLOC_SRC_DIR) && ./autogen.sh && CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --prefix=$(JEMALLOC_INSTALL_DIR) --with-pic --enable-static && make && make install

$(EVENT_LIB):
	$(Q)echo "Building libevent..."
	$(Q)mkdir -p $(EVENT_INSTALL_DIR)
	$(Q)cd $(EVENT_SRC_DIR) && ./autogen.sh && ./configure --prefix=$(EVENT_INSTALL_DIR) --with-pic --disable-shared --enable-static --disable-openssl && make && make install

$(CK_LIB):
	$(Q)echo "Building Concurrency Kit..."
	$(Q)mkdir -p $(CK_INSTALL_DIR)
	$(Q)cd $(CK_SRC_DIR) && ./configure --prefix=$(CK_INSTALL_DIR) && make && make install

# Check dependencies
check-deps:
	$(Q)echo "Checking dependencies..."
	$(Q)echo -n "  jemalloc: "; if [ -f "$(JEMALLOC_LIB)" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  libevent: "; if [ -f "$(EVENT_LIB)" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  ck: "; if [ -f "$(CK_LIB)" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  tglog static lib: "; if [ -f "$(LIB_DIR)/libtglog.a" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  tglog shared lib: "; if [ -f "$(LIB_DIR)/libtglog.so" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  tglog-st static lib: "; if [ -f "$(LIB_DIR)/libtglog-st.a" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  tglog-st shared lib: "; if [ -f "$(LIB_DIR)/libtglog-st.so" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  test-tapp: "; if [ -f "test-tapp/tglog_tapp" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  Java JNI lib: "; if [ -f "bindings/libtglog_jni.so" ]; then echo "✓ built"; else echo "✗ missing"; fi
	$(Q)echo -n "  Python bindings: "; if [ -f "bindings/tglog_client.py" ]; then echo "✓ ready"; else echo "✗ missing"; fi

# Clean targets
clean:
	$(Q)echo "Cleaning build artifacts..."
	$(Q)for DIR in $(SUB_DIRS); do \
		$(MAKE) -C $$DIR clean; \
	done
	$(Q)rm -rf $(LIB_DIR)
	$(Q)echo "✓ Clean completed!"

clean-bindings:
	$(Q)echo "Cleaning language bindings..."
	$(Q)$(MAKE) -C bindings clean
	$(Q)echo "✓ Bindings cleaned!"

clean-deps:
	$(Q)echo "Cleaning dependencies..."
	$(Q)rm -rf $(JEMALLOC_INSTALL_DIR) $(EVENT_INSTALL_DIR) $(CK_INSTALL_DIR)
	$(Q)cd $(JEMALLOC_SRC_DIR) && make clean
	$(Q)cd $(EVENT_SRC_DIR) && make clean
	$(Q)cd $(CK_SRC_DIR) && make clean
	$(Q)echo "✓ Dependencies cleaned!"

uninstall:
	$(Q)echo "Uninstalling..."
	$(Q)rm -rf dist/
	$(Q)echo "✓ Uninstall completed!"

# Install - install all components
install: all
	$(Q)echo "Installing..."
	$(Q)# Create directory structure
	$(Q)mkdir -p dist/lib dist/bin dist/include dist/bindings/java dist/bindings/python
	$(Q)mkdir -p dist/thirdparty/jemalloc dist/thirdparty/event dist/thirdparty/ck

	$(Q)# Install libraries and headers by calling subdirectory makefiles
	$(Q)echo "  Installing TGLog libraries and headers..."
	$(Q)$(MAKE) -C tglog install BUILD_SHARED=1
	$(Q)$(MAKE) -C tglog-st install BUILD_SHARED=1

	$(Q)# Install test programs
	$(Q)echo "  Installing test programs..."
	$(Q)$(MAKE) -C test install
	$(Q)$(MAKE) -C test-st install
	$(Q)$(MAKE) -C test-tapp install 2>/dev/null || true
	
	$(Q)# Install bindings by calling subdirectory makefiles
	$(Q)echo "  Installing bindings..."
	$(Q)$(MAKE) -C bindings/java install 2>/dev/null || true
	$(Q)$(MAKE) -C bindings/python install 2>/dev/null || true
	
	$(Q)# Install third-party dependencies
	$(Q)echo "  Installing third-party libraries..."
	$(Q)# jemalloc
	$(Q)if [ -d "$(JEMALLOC_INSTALL_DIR)" ]; then \
		cp -rf $(JEMALLOC_INSTALL_DIR)/lib dist/thirdparty/jemalloc/ 2>/dev/null || true; \
		cp -rf $(JEMALLOC_INSTALL_DIR)/include dist/thirdparty/jemalloc/ 2>/dev/null || true; \
	fi
	$(Q)# libevent
	$(Q)if [ -d "$(EVENT_INSTALL_DIR)" ]; then \
		cp -rf $(EVENT_INSTALL_DIR)/lib dist/thirdparty/event/ 2>/dev/null || true; \
		cp -rf $(EVENT_INSTALL_DIR)/include dist/thirdparty/event/ 2>/dev/null || true; \
	fi
	$(Q)# ck
	$(Q)if [ -d "$(CK_INSTALL_DIR)" ]; then \
		cp -rf $(CK_INSTALL_DIR)/lib dist/thirdparty/ck/ 2>/dev/null || true; \
		cp -rf $(CK_INSTALL_DIR)/include dist/thirdparty/ck/ 2>/dev/null || true; \
	fi
	
	$(Q)# Create pkg-config files
	$(Q)echo "  Creating pkg-config files..."
	$(Q)mkdir -p dist/lib/pkgconfig

	$(Q)# TGLog (multi-threaded version)
	$(Q)echo "prefix=$$(pwd)/dist" > dist/lib/pkgconfig/tglog.pc
	$(Q)echo "exec_prefix=\$${prefix}" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "libdir=\$${exec_prefix}/lib" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "includedir=\$${prefix}/include" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "Name: TGLog" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "Description: High-performance logging library (multi-threaded)" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "Version: 1.0.0" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "Libs: -L\$${libdir} -ltglog -L\$${prefix}/thirdparty/event/lib -levent -levent_pthreads -L\$${prefix}/thirdparty/ck/lib -lck -L\$${prefix}/thirdparty/jemalloc/lib -ljemalloc -lpthread" >> dist/lib/pkgconfig/tglog.pc
	$(Q)echo "Cflags: -I\$${includedir}/tglog -I\$${prefix}/thirdparty/event/include -I\$${prefix}/thirdparty/ck/include" >> dist/lib/pkgconfig/tglog.pc

	$(Q)# TGLog-ST (single-threaded version)
	$(Q)echo "prefix=$$(pwd)/dist" > dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "exec_prefix=\$${prefix}" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "libdir=\$${exec_prefix}/lib" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "includedir=\$${prefix}/include" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "Name: TGLog-ST" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "Description: High-performance logging library (single-threaded)" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "Version: 1.0.0" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "Libs: -L\$${libdir} -ltglog-st -L\$${prefix}/thirdparty/event/lib -levent -L\$${prefix}/thirdparty/ck/lib -lck -L\$${prefix}/thirdparty/jemalloc/lib -ljemalloc -lpthread" >> dist/lib/pkgconfig/tglog-st.pc
	$(Q)echo "Cflags: -I\$${includedir}/tglog-st -I\$${prefix}/thirdparty/event/include -I\$${prefix}/thirdparty/ck/include" >> dist/lib/pkgconfig/tglog-st.pc
	
	$(Q)# Create usage README
	$(Q)echo "  Creating usage documentation..."
	$(Q)echo "# TGLog SDK Distribution" > dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "## Directory Structure" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "dist/" >> dist/README.md
	$(Q)echo "├── lib/                    # Libraries" >> dist/README.md
	$(Q)echo "│   ├── libtglog.a          # Multi-threaded (static)" >> dist/README.md
	$(Q)echo "│   ├── libtglog.so         # Multi-threaded (shared)" >> dist/README.md
	$(Q)echo "│   ├── libtglog-st.a       # Single-threaded (static)" >> dist/README.md
	$(Q)echo "│   └── libtglog-st.so      # Single-threaded (shared)" >> dist/README.md
	$(Q)echo "├── include/                # Header files" >> dist/README.md
	$(Q)echo "│   ├── tglog/              # Multi-threaded version" >> dist/README.md
	$(Q)echo "│   └── tglog-st/           # Single-threaded version" >> dist/README.md
	$(Q)echo "├── bin/                    # Test programs" >> dist/README.md
	$(Q)echo "├── examples/               # Example applications" >> dist/README.md
	$(Q)echo "│   └── test-tapp/          # TAPP integration example" >> dist/README.md
	$(Q)echo "├── bindings/               # Language bindings" >> dist/README.md
	$(Q)echo "│   ├── java/               # Java JNI bindings" >> dist/README.md
	$(Q)echo "│   └── python/             # Python bindings" >> dist/README.md
	$(Q)echo "└── thirdparty/             # Dependencies" >> dist/README.md
	$(Q)echo "    ├── event/              # libevent" >> dist/README.md
	$(Q)echo "    ├── ck/                 # Concurrency Kit" >> dist/README.md
	$(Q)echo "    └── jemalloc/           # jemalloc" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "## Quick Start" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "### 1. Using Multi-threaded Version (tglog)" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "\`\`\`c" >> dist/README.md
	$(Q)echo "#include <tglog/tglog.h>" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "int main() {" >> dist/README.md
	$(Q)echo "    tglog_conf *conf = tglog_conf_create(TGLOG_NET_TCP, \"127.0.0.1\", 9999," >> dist/README.md
	$(Q)echo "                                          NULL, NULL, NULL);" >> dist/README.md
	$(Q)echo "    tglog_client *client = tglog_client_create(conf);" >> dist/README.md
	$(Q)echo "    " >> dist/README.md
	$(Q)echo "    // Log format: tglogname|field1|field2|...|fieldn\\n" >> dist/README.md
	$(Q)echo "    tglog_client_send(client, \"mylog|user123|login|success\\n\", NULL, NULL, TGLOG_MSG_F_COPY);" >> dist/README.md
	$(Q)echo "    " >> dist/README.md
	$(Q)echo "    tglog_client_destroy(client);" >> dist/README.md
	$(Q)echo "    tglog_conf_destroy(conf);" >> dist/README.md
	$(Q)echo "    return 0;" >> dist/README.md
	$(Q)echo "}" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "### 2. Using Single-threaded Version (tglog-st)" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "\`\`\`c" >> dist/README.md
	$(Q)echo "#include <tglog-st/tglog.h>" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "int main() {" >> dist/README.md
	$(Q)echo "    tglog_conf *conf = tglog_conf_create(TGLOG_NET_TCP, \"127.0.0.1\", 9999," >> dist/README.md
	$(Q)echo "                                          NULL, NULL, NULL);" >> dist/README.md
	$(Q)echo "    tglog_client *client = tglog_client_create(conf);" >> dist/README.md
	$(Q)echo "    " >> dist/README.md
	$(Q)echo "    // Log format: tglogname|field1|field2|...|fieldn\\n" >> dist/README.md
	$(Q)echo "    tglog_client_send(client, \"mylog|user123|login|success\\n\", NULL, NULL, TGLOG_MSG_F_COPY);" >> dist/README.md
	$(Q)echo "    tglog_client_update(client);  // Must call update() periodically!" >> dist/README.md
	$(Q)echo "    " >> dist/README.md
	$(Q)echo "    tglog_client_destroy(client);" >> dist/README.md
	$(Q)echo "    tglog_conf_destroy(conf);" >> dist/README.md
	$(Q)echo "    return 0;" >> dist/README.md
	$(Q)echo "}" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "## Compilation Examples" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "### Method 1: Direct Compilation (Recommended)" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "\`\`\`bash" >> dist/README.md
	$(Q)echo "# Multi-threaded version" >> dist/README.md
	$(Q)echo "gcc -o myapp myapp.c \\" >> dist/README.md
	$(Q)echo "    -Iinclude/tglog \\" >> dist/README.md
	$(Q)echo "    -Ithirdparty/event/include \\" >> dist/README.md
	$(Q)echo "    -Ithirdparty/ck/include \\" >> dist/README.md
	$(Q)echo "    -Llib -Lthirdparty/event/lib -Lthirdparty/ck/lib -Lthirdparty/jemalloc/lib \\" >> dist/README.md
	$(Q)echo "    -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "# Single-threaded version" >> dist/README.md
	$(Q)echo "gcc -o myapp myapp.c \\" >> dist/README.md
	$(Q)echo "    -Iinclude/tglog-st \\" >> dist/README.md
	$(Q)echo "    -Ithirdparty/event/include \\" >> dist/README.md
	$(Q)echo "    -Ithirdparty/ck/include \\" >> dist/README.md
	$(Q)echo "    -Llib -Lthirdparty/event/lib -Lthirdparty/ck/lib -Lthirdparty/jemalloc/lib \\" >> dist/README.md
	$(Q)echo "    -ltglog-st -levent -lck -ljemalloc -lpthread" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "### Method 2: Using pkg-config (Easiest)" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "pkg-config automatically includes all dependencies (libevent, ck, jemalloc):" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "\`\`\`bash" >> dist/README.md
	$(Q)echo "# Set PKG_CONFIG_PATH (relative to dist directory)" >> dist/README.md
	$(Q)echo "export PKG_CONFIG_PATH=\$$(pwd)/lib/pkgconfig:\$$PKG_CONFIG_PATH" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "# Multi-threaded version" >> dist/README.md
	$(Q)echo "gcc -o myapp myapp.c \`pkg-config --cflags --libs tglog\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "# Single-threaded version" >> dist/README.md
	$(Q)echo "gcc -o myapp myapp.c \`pkg-config --cflags --libs tglog-st\`" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "### Method 3: Using Makefile (Like test/ examples)" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "\`\`\`makefile" >> dist/README.md
	$(Q)echo "# Multi-threaded version" >> dist/README.md
	$(Q)echo "LIBDIR := lib thirdparty/event/lib thirdparty/ck/lib thirdparty/jemalloc/lib" >> dist/README.md
	$(Q)echo "STATIC_LIBS := tglog event event_pthreads ck jemalloc" >> dist/README.md
	$(Q)echo "DYNAMIC_LIBS := pthread" >> dist/README.md
	$(Q)echo "INCLUDES := include/tglog thirdparty/event/include thirdparty/ck/include" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "myapp: myapp.c" >> dist/README.md
	$(Q)echo "	gcc \$$(addprefix -I,\$$(INCLUDES)) -o \$$@ \$$< \\" >> dist/README.md
	$(Q)echo "	    \$$(addprefix -L,\$$(LIBDIR)) \\" >> dist/README.md
	$(Q)echo "	    \$$(addprefix -l,\$$(STATIC_LIBS)) \\" >> dist/README.md
	$(Q)echo "	    \$$(addprefix -l,\$$(DYNAMIC_LIBS))" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "# Single-threaded version: replace tglog with tglog-st, include/tglog with include/tglog-st" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "## Key Differences" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "| Feature | Multi-threaded (tglog) | Single-threaded (tglog-st) |" >> dist/README.md
	$(Q)echo "|---------|------------------------|----------------------------|" >> dist/README.md
	$(Q)echo "| Performance | ~1M logs/sec | ~100K-200K logs/sec |" >> dist/README.md
	$(Q)echo "| Worker Thread | Yes (automatic) | No (manual update) |" >> dist/README.md
	$(Q)echo "| Update Call | Not needed | **Required** \`tglog_client_update()\` |" >> dist/README.md
	$(Q)echo "| Memory Usage | Higher | Lower |" >> dist/README.md
	$(Q)echo "| Use Case | High-throughput servers | Single-threaded apps, embedded |" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "## Examples" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "### Test Programs" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "See \`bin/\` directory for basic test programs:" >> dist/README.md
	$(Q)echo "- Multi-threaded: \`tglog_test\`" >> dist/README.md
	$(Q)echo "- Single-threaded: \`tglog_test_st\`, \`simple_perf_test_st\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "Run test programs:" >> dist/README.md
	$(Q)echo "\`\`\`bash" >> dist/README.md
	$(Q)echo "cd bin" >> dist/README.md
	$(Q)echo "./tglog_test           # Multi-threaded test" >> dist/README.md
	$(Q)echo "./tglog_test_st        # Single-threaded test" >> dist/README.md
	$(Q)echo "./simple_perf_test_st  # Single-threaded performance test" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "### TAPP Integration Example" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "See \`examples/test-tapp/\` for a complete TAPP application example:" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "\`\`\`bash" >> dist/README.md
	$(Q)echo "cd examples/test-tapp" >> dist/README.md
	$(Q)echo "./start.sh    # Start the TAPP application" >> dist/README.md
	$(Q)echo "./stop.sh     # Stop the TAPP application" >> dist/README.md
	$(Q)echo "\`\`\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "## Language Bindings" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "- **Java**: See \`bindings/java/README_JAVA.md\`" >> dist/README.md
	$(Q)echo "- **Python**: See \`bindings/python/README_PYTHON.md\`" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "## Documentation" >> dist/README.md
	$(Q)echo "" >> dist/README.md
	$(Q)echo "For detailed documentation, see the source repository:" >> dist/README.md
	$(Q)echo "- \`tglog/DESIGN.md\` - Multi-threaded version design" >> dist/README.md
	$(Q)echo "- \`tglog-st/DESIGN.md\` - Single-threaded version design" >> dist/README.md
	
	$(Q)echo "✓ Installation completed in dist/"
	$(Q)echo "  Libraries:"
	$(Q)echo "    - dist/lib/libtglog.a (multi-threaded static)"
	$(Q)echo "    - dist/lib/libtglog.so (multi-threaded shared)"
	$(Q)echo "    - dist/lib/libtglog-st.a (single-threaded static)"
	$(Q)echo "    - dist/lib/libtglog-st.so (single-threaded shared)"
	$(Q)echo "  Headers:"
	$(Q)echo "    - dist/include/tglog/ (multi-threaded)"
	$(Q)echo "    - dist/include/tglog-st/ (single-threaded)"
	$(Q)echo "  Binaries: dist/bin/"
	$(Q)echo "  Bindings: dist/bindings/"
	$(Q)echo "  Dependencies: dist/thirdparty/"

# Help target
help:
	@echo "TGLog SDK Build System"
	@echo "======================"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build everything (dependencies + TGLog + tests + bindings)"
	@echo "  init-submodules - Initialize and checkout git submodules to specific versions"
	@echo "  deps         - Build only dependencies (jemalloc, libevent, ck)"
	@echo "  tglog        - Build TGLog library (static and shared)"
	@echo "  tglog-st     - Build TGLog single-threaded library"
	@echo "  test         - Build test programs"
	@echo "  test-tapp    - Build test-tapp program (TSF4G integration)"
	@echo "  examples     - Alias for 'test'"
	@echo "  bindings     - Build all language bindings (Java + Python)"
	@echo "  java         - Build only Java bindings"
	@echo "  python       - Build only Python bindings"
	@echo "  clean        - Clean build artifacts"
	@echo "  clean-deps   - Clean and remove dependencies"
	@echo "  clean-bindings - Clean language bindings"
	@echo "  uninstall    - Remove installation directory (dist/)"
	@echo "  check-deps   - Check status of dependencies and bindings"
	@echo "  install      - Install all artifacts to dist/"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Build options:"
	@echo "  RELEASE=1  - Build optimized release version"
	@echo "  V=1        - Verbose build output"
	@echo ""
	@echo "Submodule versions:"
	@echo "  jemalloc: $(JEMALLOC_VERSION)"
	@echo "  libevent: $(EVENT_VERSION)"
	@echo "  ck:       $(CK_VERSION)"
	@echo ""
	@echo "Examples:"
	@echo "  make               # Build everything"
	@echo "  make init-submodules # Initialize git submodules"
	@echo "  make deps          # Build only dependencies"
	@echo "  make tglog         # Build TGLog library only"
	@echo "  make test-tapp     # Build test-tapp only"
	@echo "  make bindings      # Build language bindings"
	@echo "  make java          # Build Java bindings only"
	@echo "  make python        # Build Python bindings only"
	@echo "  make RELEASE=1     # Build release version"
	@echo "  make V=1           # Verbose build"
