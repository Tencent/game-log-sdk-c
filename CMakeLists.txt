cmake_minimum_required(VERSION 3.10)
project(sdk_c C)

set(CMAKE_C_STANDARD 99)

# Generate compile_commands.json for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置当前目录变量
set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 设置子模块路径和版本
set(JEMALLOC_SRC_DIR ${CURRENT_DIR}/jemalloc)
set(JEMALLOC_INSTALL_DIR ${CURRENT_DIR}/thirdparty/jemalloc)
set(JEMALLOC_LIB ${JEMALLOC_INSTALL_DIR}/lib/libjemalloc.a)
set(JEMALLOC_VERSION "5.3.0")

set(EVENT_SRC_DIR ${CURRENT_DIR}/event)
set(EVENT_INSTALL_DIR ${CURRENT_DIR}/thirdparty/event)
set(EVENT_LIB ${EVENT_INSTALL_DIR}/lib/libevent.a)
set(EVENT_VERSION "release-2.1.12-stable")

set(CK_SRC_DIR ${CURRENT_DIR}/ck)
set(CK_INSTALL_DIR ${CURRENT_DIR}/thirdparty/ck)
set(CK_LIB ${CK_INSTALL_DIR}/lib/libck.a)
set(CK_VERSION "0.7.2")

set(LIB_DIR ${CURRENT_DIR}/lib)

# 编译器设置
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -D_GNU_SOURCE -D_REENTRANT -Wno-typedef-redefinition")

# 构建优化选项
if(RELEASE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -DDEBUG")
endif()

# 默认构建动态库
option(BUILD_SHARED "Build shared library" ON)

# 创建必要的目录
file(MAKE_DIRECTORY ${JEMALLOC_INSTALL_DIR})
file(MAKE_DIRECTORY ${EVENT_INSTALL_DIR})
file(MAKE_DIRECTORY ${CK_INSTALL_DIR})
file(MAKE_DIRECTORY ${LIB_DIR})

# 检查依赖是否存在
message(STATUS "Checking dependencies...")

# 检查目录是否存在且不为空
file(GLOB JEMALLOC_FILES "${JEMALLOC_SRC_DIR}/*")
file(GLOB EVENT_FILES "${EVENT_SRC_DIR}/*")
file(GLOB CK_FILES "${CK_SRC_DIR}/*")

if(NOT JEMALLOC_FILES OR NOT EVENT_FILES OR NOT CK_FILES)
    message(STATUS "Dependencies not found, initializing...")

    # 检查是否在 git 仓库中
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
        message(STATUS "Using git submodules...")
        execute_process(
            COMMAND git submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT
            ERROR_VARIABLE GIT_SUBMOD_ERROR
        )
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}\n${GIT_SUBMOD_ERROR}")
        endif()
    else()
        message(STATUS "Running download-deps.sh to download dependencies...")
        execute_process(
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/download-deps.sh
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE DOWNLOAD_RESULT
        )
        if(NOT DOWNLOAD_RESULT EQUAL "0")
            message(FATAL_ERROR "Failed to download dependencies. Please check download-deps.sh output.")
        endif()
    endif()
else()
    message(STATUS "Dependencies found")
endif()

# 版本检查（只在 git 仓库中进行）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
    message(STATUS "Checking out submodules to specific versions...")

    # Check and checkout jemalloc
    if(EXISTS ${JEMALLOC_SRC_DIR}/.git)
        execute_process(
            COMMAND git describe --tags --exact-match
            WORKING_DIRECTORY ${JEMALLOC_SRC_DIR}
            OUTPUT_VARIABLE JEMALLOC_CURRENT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT JEMALLOC_CURRENT STREQUAL JEMALLOC_VERSION)
            message(STATUS "  jemalloc: ${JEMALLOC_CURRENT} -> ${JEMALLOC_VERSION}")
            execute_process(
                COMMAND git checkout ${JEMALLOC_VERSION}
                WORKING_DIRECTORY ${JEMALLOC_SRC_DIR}
                OUTPUT_QUIET ERROR_QUIET
            )
        endif()
    endif()

    # Check and checkout libevent
    if(EXISTS ${EVENT_SRC_DIR}/.git)
        execute_process(
            COMMAND git describe --tags --exact-match
            WORKING_DIRECTORY ${EVENT_SRC_DIR}
            OUTPUT_VARIABLE EVENT_CURRENT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT EVENT_CURRENT STREQUAL EVENT_VERSION)
            message(STATUS "  libevent: ${EVENT_CURRENT} -> ${EVENT_VERSION}")
            execute_process(
                COMMAND git checkout ${EVENT_VERSION}
                WORKING_DIRECTORY ${EVENT_SRC_DIR}
                OUTPUT_QUIET ERROR_QUIET
            )
        endif()
    endif()

    # Check and checkout ck
    if(EXISTS ${CK_SRC_DIR}/.git)
        execute_process(
            COMMAND git describe --tags --exact-match
            WORKING_DIRECTORY ${CK_SRC_DIR}
            OUTPUT_VARIABLE CK_CURRENT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT CK_CURRENT STREQUAL CK_VERSION)
            message(STATUS "  ck: ${CK_CURRENT} -> ${CK_VERSION}")
            execute_process(
                COMMAND git checkout ${CK_VERSION}
                WORKING_DIRECTORY ${CK_SRC_DIR}
                OUTPUT_QUIET ERROR_QUIET
            )
        endif()
    endif()

    message(STATUS "Submodules checked out to specified versions")
else()
    message(STATUS "Using downloaded dependencies (version check skipped)")
endif()

# 使用 ExternalProject 来构建依赖项
include(ExternalProject)

# 构建 jemalloc
if(NOT EXISTS ${JEMALLOC_LIB})
    ExternalProject_Add(
        jemalloc_build
        SOURCE_DIR ${JEMALLOC_SRC_DIR}
        CONFIGURE_COMMAND cd ${JEMALLOC_SRC_DIR} && ./autogen.sh && CFLAGS=-fPIC CXXFLAGS=-fPIC ./configure --prefix=${JEMALLOC_INSTALL_DIR} --with-pic --enable-static
        BUILD_COMMAND cd ${JEMALLOC_SRC_DIR} && ${CMAKE_MAKE_PROGRAM}
        INSTALL_COMMAND cd ${JEMALLOC_SRC_DIR} && ${CMAKE_MAKE_PROGRAM} install
        BUILD_IN_SOURCE 1
        BUILD_ALWAYS 0
    )
else()
    # 如果库已存在，创建一个空目标
    add_custom_target(jemalloc_build)
endif()

# 构建 libevent
if(NOT EXISTS ${EVENT_LIB})
    ExternalProject_Add(
        libevent_build
        SOURCE_DIR ${EVENT_SRC_DIR}
        CONFIGURE_COMMAND cd ${EVENT_SRC_DIR} && ./autogen.sh && ./configure --prefix=${EVENT_INSTALL_DIR} --with-pic --disable-shared --enable-static --disable-openssl
        BUILD_COMMAND cd ${EVENT_SRC_DIR} && ${CMAKE_MAKE_PROGRAM}
        INSTALL_COMMAND cd ${EVENT_SRC_DIR} && ${CMAKE_MAKE_PROGRAM} install
        BUILD_IN_SOURCE 1
        BUILD_ALWAYS 0
    )
else()
    # 如果库已存在，创建一个空目标
    add_custom_target(libevent_build)
endif()

# 构建 Concurrency Kit
if(NOT EXISTS ${CK_LIB})
    ExternalProject_Add(
        ck_build
        SOURCE_DIR ${CK_SRC_DIR}
        CONFIGURE_COMMAND cd ${CK_SRC_DIR} && ./configure --prefix=${CK_INSTALL_DIR}
        BUILD_COMMAND cd ${CK_SRC_DIR} && make
        INSTALL_COMMAND cd ${CK_SRC_DIR} && make install
        BUILD_IN_SOURCE 1
        BUILD_ALWAYS 0
    )
else()
    # 如果库已存在，创建一个空目标
    add_custom_target(ck_build)
endif()

# 导入构建的库作为 IMPORTED 目标
add_library(jemalloc STATIC IMPORTED)
set_target_properties(jemalloc PROPERTIES IMPORTED_LOCATION ${JEMALLOC_LIB})
add_dependencies(jemalloc jemalloc_build)

add_library(event STATIC IMPORTED)
set_target_properties(event PROPERTIES
    IMPORTED_LOCATION ${EVENT_LIB}
)
# 使用生成器表达式，只在构建时添加 include 目录，避免配置时检查路径
target_include_directories(event INTERFACE
    $<BUILD_INTERFACE:${EVENT_SRC_DIR}>
    $<BUILD_INTERFACE:${EVENT_SRC_DIR}/include>
    $<BUILD_INTERFACE:${EVENT_INSTALL_DIR}/include>
)
add_dependencies(event libevent_build)

add_library(event_pthreads STATIC IMPORTED)
set_target_properties(event_pthreads PROPERTIES
    IMPORTED_LOCATION ${EVENT_INSTALL_DIR}/lib/libevent_pthreads.a
)
target_include_directories(event_pthreads INTERFACE
    $<BUILD_INTERFACE:${EVENT_SRC_DIR}>
    $<BUILD_INTERFACE:${EVENT_SRC_DIR}/include>
    $<BUILD_INTERFACE:${EVENT_INSTALL_DIR}/include>
)
add_dependencies(event_pthreads libevent_build)

# 添加 event 源文件索引，让 CLion 能够识别和跳转
file(GLOB_RECURSE EVENT_SOURCES
    ${EVENT_SRC_DIR}/*.c
    ${EVENT_SRC_DIR}/*.h
)
add_library(event_sources INTERFACE)
target_sources(event_sources INTERFACE ${EVENT_SOURCES})
target_include_directories(event_sources INTERFACE
    $<BUILD_INTERFACE:${EVENT_SRC_DIR}>
    $<BUILD_INTERFACE:${EVENT_SRC_DIR}/include>
    $<BUILD_INTERFACE:${EVENT_INSTALL_DIR}/include>
)

add_library(ck STATIC IMPORTED)
set_target_properties(ck PROPERTIES IMPORTED_LOCATION ${CK_LIB})
target_include_directories(ck INTERFACE
    $<BUILD_INTERFACE:${CK_INSTALL_DIR}/include>
)
add_dependencies(ck ck_build)

# 在添加子目录之前，确保依赖库的目录存在
# 这样可以避免 CMake 配置时检查路径不存在的错误
# 注意：这些目录会在实际构建依赖库时被填充

# 创建占位目录（如果不存在）
file(MAKE_DIRECTORY ${EVENT_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${CK_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${JEMALLOC_INSTALL_DIR}/include)

# 添加子目录
add_subdirectory(tglog)
add_subdirectory(tglog-st)
add_subdirectory(test)
add_subdirectory(test-st)
add_subdirectory(test-tapp)

# 添加bindings目录（如果存在）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bindings/CMakeLists.txt)
    add_subdirectory(bindings)
else()
    # 如果bindings没有CMakeLists.txt，使用自定义命令
    add_custom_target(bindings
        COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/bindings all
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings
        COMMENT "Building language bindings..."
        DEPENDS tglog
    )

    add_custom_target(python
        COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/bindings python
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings
        COMMENT "Building Python bindings..."
        DEPENDS tglog
    )
    
    add_custom_target(clean-bindings
        COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/bindings clean
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings
        COMMENT "Cleaning language bindings..."
    )
endif()

# 主目标
add_custom_target(deps
    DEPENDS jemalloc_build libevent_build ck_build
    COMMENT "Building dependencies..."
)

# 创建一个 all_deps 目标，确保所有子项目都依赖它
add_custom_target(all_deps ALL
    DEPENDS jemalloc_build libevent_build ck_build
    COMMENT "Ensuring all dependencies are built..."
)

# 清理依赖项
add_custom_target(clean-deps
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${JEMALLOC_INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${EVENT_INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CK_INSTALL_DIR}
    COMMAND cd ${JEMALLOC_SRC_DIR} && make clean
    COMMAND cd ${EVENT_SRC_DIR} && make clean
    COMMAND cd ${CK_SRC_DIR} && make clean
    COMMENT "Cleaning dependencies..."
)

# 卸载目标
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_SOURCE_DIR}/dist
    COMMENT "Uninstalling..."
)

# 检查依赖项
add_custom_target(check-deps
    COMMAND ${CMAKE_COMMAND} -E echo "Checking dependencies..."
    COMMAND ${CMAKE_COMMAND} -E echo -n "  jemalloc: " 
    COMMAND test -f ${JEMALLOC_LIB} && echo "✓ built" || echo "✗ missing"
    COMMAND ${CMAKE_COMMAND} -E echo -n "  libevent: "
    COMMAND test -f ${EVENT_LIB} && echo "✓ built" || echo "✗ missing"
    COMMAND ${CMAKE_COMMAND} -E echo -n "  ck: "
    COMMAND test -f ${CK_LIB} && echo "✓ built" || echo "✗ missing"
    COMMAND ${CMAKE_COMMAND} -E echo -n "  tglog static lib: "
    COMMAND test -f ${LIB_DIR}/libtglog.a && echo "✓ built" || echo "✗ missing"
    COMMAND ${CMAKE_COMMAND} -E echo -n "  tglog shared lib: "
    COMMAND test -f ${LIB_DIR}/libtglog.so && echo "✓ built" || echo "✗ missing"
    COMMAND ${CMAKE_COMMAND} -E echo -n "  tglog-st static lib: "
    COMMAND test -f ${LIB_DIR}/libtglog-st.a && echo "✓ built" || echo "✗ missing"
    COMMAND ${CMAKE_COMMAND} -E echo -n "  tglog-st shared lib: "
    COMMAND test -f ${LIB_DIR}/libtglog-st.so && echo "✓ built" || echo "✗ missing"
    COMMAND ${CMAKE_COMMAND} -E echo -n "  Python bindings: "
    COMMAND test -f ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/tglog_client.py && echo "✓ ready" || echo "✗ missing"
    COMMENT "Checking dependencies status..."
)

# 帮助目标
add_custom_target(help_info
    COMMAND ${CMAKE_COMMAND} -E echo "TGLog SDK Build System"
    COMMAND ${CMAKE_COMMAND} -E echo "======================"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  all          - Build everything (dependencies + TGLog + tests + bindings)"
    COMMAND ${CMAKE_COMMAND} -E echo "  deps         - Build only dependencies (jemalloc, libevent, ck)"
    COMMAND ${CMAKE_COMMAND} -E echo "  tglog        - Build TGLog library (static and shared)"
    COMMAND ${CMAKE_COMMAND} -E echo "  tglog-st     - Build TGLog single-threaded library"
    COMMAND ${CMAKE_COMMAND} -E echo "  test         - Build test programs"
    COMMAND ${CMAKE_COMMAND} -E echo "  test-st      - Build single-threaded test programs"
    COMMAND ${CMAKE_COMMAND} -E echo "  test-tapp    - Build TAPP test programs"
    COMMAND ${CMAKE_COMMAND} -E echo "  examples     - Alias for 'test'"
    COMMAND ${CMAKE_COMMAND} -E echo "  bindings     - Build all language bindings (Python)"
    COMMAND ${CMAKE_COMMAND} -E echo "  python       - Build only Python bindings"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean        - Clean build artifacts"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean-deps   - Clean and remove dependencies"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean-bindings - Clean language bindings"
    COMMAND ${CMAKE_COMMAND} -E echo "  uninstall    - Remove installation directory (dist/)"
    COMMAND ${CMAKE_COMMAND} -E echo "  check-deps   - Check status of dependencies and bindings"
    COMMAND ${CMAKE_COMMAND} -E echo "  install      - Install built artifacts"
    COMMAND ${CMAKE_COMMAND} -E echo "  help_info    - Show this help message"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Build options:"
    COMMAND ${CMAKE_COMMAND} -E echo "  RELEASE=1  - Build optimized release version"
    COMMAND ${CMAKE_COMMAND} -E echo "  V=1        - Verbose build output"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Submodule versions (auto-initialized):"
    COMMAND ${CMAKE_COMMAND} -E echo "  jemalloc: ${JEMALLOC_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "  libevent: ${EVENT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "  ck:       ${CK_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Examples:"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake -DRELEASE=1 .   # Configure release build"
    COMMAND ${CMAKE_COMMAND} -E echo "  make                   # Build everything"
    COMMAND ${CMAKE_COMMAND} -E echo "  make deps              # Build only dependencies"
    COMMAND ${CMAKE_COMMAND} -E echo "  make tglog             # Build TGLog library only"
    COMMAND ${CMAKE_COMMAND} -E echo "  make bindings          # Build language bindings"
    COMMAND ${CMAKE_COMMAND} -E echo "  make python            # Build Python bindings only"
    COMMAND ${CMAKE_COMMAND} -E echo "  make VERBOSE=1         # Verbose build"
)

# 设置别名目标
add_custom_target(examples DEPENDS test_programs)

# 设置安装规则
# 设置安装目录为项目目录下的dist
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})

# 注意：库和头文件的安装由子目录的 CMakeLists.txt 处理
# 这里只处理测试程序、bindings 和第三方库的安装

# 安装测试程序（多线程版本）
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test/
    DESTINATION dist/bin
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*_test"
    PATTERN "comprehensive_test"
    PATTERN "*.c" EXCLUDE
    PATTERN "*.o" EXCLUDE
    PATTERN "*.d" EXCLUDE
    PATTERN "CMakeFiles" EXCLUDE
    PATTERN "Makefile" EXCLUDE
    PATTERN "cmake_install.cmake" EXCLUDE
    PATTERN "CMakeLists.txt" EXCLUDE
)

# 安装测试程序（单线程版本）
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test-st/
    DESTINATION dist/bin
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING
    PATTERN "*_st"
    PATTERN "*.c" EXCLUDE
    PATTERN "*.o" EXCLUDE
    PATTERN "*.d" EXCLUDE
    PATTERN "CMakeFiles" EXCLUDE
    PATTERN "Makefile" EXCLUDE
    PATTERN "cmake_install.cmake" EXCLUDE
    PATTERN "CMakeLists.txt" EXCLUDE
)

# 安装test-tapp示例程序及配套文件
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/test-tapp/tglog_tapp
    DESTINATION dist/examples/test-tapp
    OPTIONAL
)
install(PROGRAMS
    ${CMAKE_CURRENT_SOURCE_DIR}/test-tapp/start.sh
    ${CMAKE_CURRENT_SOURCE_DIR}/test-tapp/stop.sh
    DESTINATION dist/examples/test-tapp
    OPTIONAL
)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/test-tapp/tglog_tapp_cfg.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/test-tapp/tlog_cfg.xml
    DESTINATION dist/examples/test-tapp
    OPTIONAL
)

# 安装Python bindings
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/
    DESTINATION dist/bindings/python
    FILES_MATCHING PATTERN "*.py" PATTERN "*.pyx" PATTERN "*.c"
    PATTERN "TglogJNI.c" EXCLUDE
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/README_PYTHON.md
    DESTINATION dist/bindings/python
    OPTIONAL
)

# 安装第三方依赖
# jemalloc
if(EXISTS ${JEMALLOC_INSTALL_DIR})
    install(DIRECTORY ${JEMALLOC_INSTALL_DIR}/lib/
        DESTINATION dist/thirdparty/jemalloc/lib
    )
    install(DIRECTORY ${JEMALLOC_INSTALL_DIR}/include/
        DESTINATION dist/thirdparty/jemalloc/include
    )
endif()

# libevent
if(EXISTS ${EVENT_INSTALL_DIR})
    install(DIRECTORY ${EVENT_INSTALL_DIR}/lib/
        DESTINATION dist/thirdparty/event/lib
    )
    install(DIRECTORY ${EVENT_INSTALL_DIR}/include/
        DESTINATION dist/thirdparty/event/include
    )
endif()

# ck
if(EXISTS ${CK_INSTALL_DIR})
    install(DIRECTORY ${CK_INSTALL_DIR}/lib/
        DESTINATION dist/thirdparty/ck/lib
    )
    install(DIRECTORY ${CK_INSTALL_DIR}/include/
        DESTINATION dist/thirdparty/ck/include
    )
endif()

# 创建pkg-config文件
# TGLog (multi-threaded version)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/tglog.pc"
"prefix=${CMAKE_CURRENT_SOURCE_DIR}/dist
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: TGLog
Description: High-performance logging library (multi-threaded)
Version: 1.0.0
Libs: -L\${libdir} -ltglog -L\${prefix}/thirdparty/event/lib -levent -levent_pthreads -L\${prefix}/thirdparty/ck/lib -lck -L\${prefix}/thirdparty/jemalloc/lib -ljemalloc -lpthread
Cflags: -I\${includedir}/tglog -I\${prefix}/thirdparty/event/include -I\${prefix}/thirdparty/ck/include
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tglog.pc
    DESTINATION dist/lib/pkgconfig
)

# TGLog-ST (single-threaded version)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/tglog-st.pc"
"prefix=${CMAKE_CURRENT_SOURCE_DIR}/dist
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: TGLog-ST
Description: High-performance logging library (single-threaded)
Version: 1.0.0
Libs: -L\${libdir} -ltglog-st -L\${prefix}/thirdparty/event/lib -levent -L\${prefix}/thirdparty/ck/lib -lck -L\${prefix}/thirdparty/jemalloc/lib -ljemalloc -lpthread
Cflags: -I\${includedir}/tglog-st -I\${prefix}/thirdparty/event/include -I\${prefix}/thirdparty/ck/include
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tglog-st.pc
    DESTINATION dist/lib/pkgconfig
)

# 创建README文件
install(CODE "
    file(WRITE \${CMAKE_INSTALL_PREFIX}/dist/README.md \"# TGLog SDK Distribution

## Directory Structure

\\\`\\\`\\\`
dist/
├── lib/                    # Libraries
│   ├── libtglog.a          # Multi-threaded (static)
│   ├── libtglog.so         # Multi-threaded (shared)
│   ├── libtglog-st.a       # Single-threaded (static)
│   └── libtglog-st.so      # Single-threaded (shared)
├── include/                # Header files
│   ├── tglog/              # Multi-threaded version
│   └── tglog-st/           # Single-threaded version
├── bin/                    # Test programs
├── examples/               # Example applications
│   └── test-tapp/          # TAPP integration example
├── bindings/               # Language bindings
│   └── python/             # Python bindings
└── thirdparty/             # Dependencies
    ├── event/              # libevent
    ├── ck/                 # Concurrency Kit
    └── jemalloc/           # jemalloc
\\\`\\\`\\\`

## Quick Start

### 1. Using Multi-threaded Version (tglog)

\\\`\\\`\\\`c
#include <tglog/tglog.h>

int main() {
    tglog_conf *conf = tglog_conf_create(TGLOG_NET_TCP, \\\"127.0.0.1\\\", 9999,
                                          NULL, NULL, NULL);
    tglog_client *client = tglog_client_create(conf);

    // Log format: tglogname|field1|field2|...|fieldn\\\\n
    tglog_client_send(client, \\\"mylog|user123|login|success\\\\n\\\", NULL, NULL, TGLOG_MSG_F_COPY);

    tglog_client_destroy(client);
    tglog_conf_destroy(conf);
    return 0;
}
\\\`\\\`\\\`

### 2. Using Single-threaded Version (tglog-st)

\\\`\\\`\\\`c
#include <tglog-st/tglog.h>

int main() {
    tglog_conf *conf = tglog_conf_create(TGLOG_NET_TCP, \\\"127.0.0.1\\\", 9999,
                                          NULL, NULL, NULL);
    tglog_client *client = tglog_client_create(conf);

    // Log format: tglogname|field1|field2|...|fieldn\\\\n
    tglog_client_send(client, \\\"mylog|user123|login|success\\\\n\\\", NULL, NULL, TGLOG_MSG_F_COPY);
    tglog_client_update(client);  // Must call update() periodically!

    tglog_client_destroy(client);
    tglog_conf_destroy(conf);
    return 0;
}
\\\`\\\`\\\`

## Compilation Examples

### Method 1: Direct Compilation (Recommended)

\\\`\\\`\\\`bash
# Multi-threaded version
gcc -o myapp myapp.c \\\\
    -Iinclude/tglog \\\\
    -Ithirdparty/event/include \\\\
    -Ithirdparty/ck/include \\\\
    -Llib -Lthirdparty/event/lib -Lthirdparty/ck/lib -Lthirdparty/jemalloc/lib \\\\
    -ltglog -levent -levent_pthreads -lck -ljemalloc -lpthread

# Single-threaded version
gcc -o myapp myapp.c \\\\
    -Iinclude/tglog-st \\\\
    -Ithirdparty/event/include \\\\
    -Ithirdparty/ck/include \\\\
    -Llib -Lthirdparty/event/lib -Lthirdparty/ck/lib -Lthirdparty/jemalloc/lib \\\\
    -ltglog-st -levent -lck -ljemalloc -lpthread
\\\`\\\`\\\`

### Method 2: Using pkg-config (Easiest)

pkg-config automatically includes all dependencies (libevent, ck, jemalloc):

\\\`\\\`\\\`bash
# Set PKG_CONFIG_PATH (relative to dist directory)
export PKG_CONFIG_PATH=\\\$(pwd)/lib/pkgconfig:\\\$PKG_CONFIG_PATH

# Multi-threaded version
gcc -o myapp myapp.c \`pkg-config --cflags --libs tglog\`

# Single-threaded version
gcc -o myapp myapp.c \`pkg-config --cflags --libs tglog-st\`
\\\`\\\`\\\`

### Method 3: Using Makefile (Like test/ examples)

\\\`\\\`\\\`makefile
# Multi-threaded version
LIBDIR := lib thirdparty/event/lib thirdparty/ck/lib thirdparty/jemalloc/lib
STATIC_LIBS := tglog event event_pthreads ck jemalloc
DYNAMIC_LIBS := pthread
INCLUDES := include/tglog thirdparty/event/include thirdparty/ck/include

myapp: myapp.c
	gcc \\\$(addprefix -I,\\\$(INCLUDES)) -o \\\$@ \\\$< \\\\
	    \\\$(addprefix -L,\\\$(LIBDIR)) \\\\
	    \\\$(addprefix -l,\\\$(STATIC_LIBS)) \\\\
	    \\\$(addprefix -l,\\\$(DYNAMIC_LIBS))

# Single-threaded version: replace tglog with tglog-st, include/tglog with include/tglog-st
\\\`\\\`\\\`

## Key Differences

| Feature | Multi-threaded (tglog) | Single-threaded (tglog-st) |
|---------|------------------------|----------------------------|
| Performance | ~1M logs/sec | ~100K-200K logs/sec |
| Worker Thread | Yes (automatic) | No (manual update) |
| Update Call | Not needed | **Required** \\\`tglog_client_update()\\\` |
| Memory Usage | Higher | Lower |
| Use Case | High-throughput servers | Single-threaded apps, embedded |

## Examples

### Test Programs

See \\\`bin/\\\` directory for basic test programs:
- Multi-threaded: \\\`tglog_test\\\`
- Single-threaded: \\\`tglog_test_st\\\`, \\\`simple_perf_test_st\\\`

Run test programs:
\\\`\\\`\\\`bash
cd bin
./tglog_test           # Multi-threaded test
./tglog_test_st        # Single-threaded test
./simple_perf_test_st  # Single-threaded performance test
\\\`\\\`\\\`

### TAPP Integration Example

See \\\`examples/test-tapp/\\\` for a complete TAPP application example:

\\\`\\\`\\\`bash
cd examples/test-tapp
./start.sh    # Start the TAPP application
./stop.sh     # Stop the TAPP application
\\\`\\\`\\\`

## Language Bindings

- **Python**: See \\\`bindings/python/README_PYTHON.md\\\`

## Documentation

For detailed documentation, see the source repository:
- \\\`tglog/DESIGN.md\\\` - Multi-threaded version design
- \\\`tglog-st/DESIGN.md\\\` - Single-threaded version design
\")
")