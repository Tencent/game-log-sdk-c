cmake_minimum_required(VERSION 3.10)
project(sdk_c C)

set(CMAKE_C_STANDARD 99)

# Generate compile_commands.json for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

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

set(LIB_DIR ${CURRENT_DIR}/lib)

# 编译器设置
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /D_CRT_SECURE_NO_WARNINGS /utf-8")
    if(RELEASE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /DNDEBUG")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Od /Zi /DDEBUG")
    endif()
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -D_GNU_SOURCE -D_REENTRANT -Wno-typedef-redefinition")
    if(RELEASE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -DDEBUG")
    endif()
endif()

# 默认构建动态库
option(BUILD_SHARED "Build shared library" ON)

# 创建必要的目录
file(MAKE_DIRECTORY ${JEMALLOC_INSTALL_DIR})
file(MAKE_DIRECTORY ${EVENT_INSTALL_DIR})
file(MAKE_DIRECTORY ${LIB_DIR})

# 检查依赖是否存在
message(STATUS "Checking dependencies...")

# 检查目录是否存在且不为空
file(GLOB JEMALLOC_FILES "${JEMALLOC_SRC_DIR}/*")
file(GLOB EVENT_FILES "${EVENT_SRC_DIR}/*")
if(NOT JEMALLOC_FILES OR NOT EVENT_FILES)
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
        if(WIN32)
            message(FATAL_ERROR "Dependencies not found. Please run: git submodule update --init --recursive")
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

    message(STATUS "Submodules checked out to specified versions")
else()
    message(STATUS "Using downloaded dependencies (version check skipped)")
endif()

# 卸载目标 - 必须在 add_subdirectory(event) 之前定义，否则 libevent 会抢先注册同名目标
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_SOURCE_DIR}/dist
    COMMENT "Uninstalling..."
)

# 使用 ExternalProject 或 add_subdirectory 来构建依赖项
if(WIN32)
    # Windows 上不编译 jemalloc，保留空目标供子目录的 add_dependencies 引用
    add_custom_target(jemalloc_build)

    set(EVENT__DISABLE_TESTS ON CACHE BOOL "Disable tests" FORCE)
    set(EVENT__DISABLE_BENCHMARKS ON CACHE BOOL "Disable benchmarks" FORCE)
    set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "Disable samples" FORCE)
    set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "Disable openssl" FORCE)
    set(EVENT__LIBRARY_TYPE STATIC CACHE STRING "Build static libevent" FORCE)
    set(EVENT__MSVC_STATIC_RUNTIME OFF CACHE BOOL "Use DLL runtime for libevent" FORCE)

    # 禁止 libevent 的 install() 规则污染本项目的安装目录
    # （libevent 默认会把头文件装到 CMAKE_INSTALL_PREFIX/include）
    # EXCLUDE_FROM_ALL: 不把 libevent 的 install 规则写入父目录的 cmake_install.cmake，
    # 但依赖关系（event_static 等）仍然正常工作
    add_subdirectory(${EVENT_SRC_DIR} event_build EXCLUDE_FROM_ALL)
    set(CMAKE_SKIP_INSTALL_RULES FALSE)

    # 构建后将 libevent 安装到 thirdparty/event（与 Linux 一致），
    # 这样 cmake --install 只需从 thirdparty/ 拷贝，Windows/Linux 逻辑统一
    add_custom_target(libevent_install_thirdparty
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EVENT_INSTALL_DIR}/lib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EVENT_INSTALL_DIR}/include
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:event_static>
                ${EVENT_INSTALL_DIR}/lib/event.lib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:event_core_static>
                ${EVENT_INSTALL_DIR}/lib/event_core.lib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:event_extra_static>
                ${EVENT_INSTALL_DIR}/lib/event_extra.lib
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${EVENT_SRC_DIR}/include
                ${EVENT_INSTALL_DIR}/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_BINARY_DIR}/event_build/include
                ${EVENT_INSTALL_DIR}/include
        DEPENDS event_core_static event_extra_static event_static
        COMMENT "Installing libevent to thirdparty/event..."
    )

    add_custom_target(libevent_build)
    add_dependencies(libevent_build libevent_install_thirdparty)

    # 保留 libevent 自己生成的 target 依赖关系：
    # event -> event_static。不要把 event 覆盖成裸文件路径，否则 VS/MSBuild
    # 只能看到 thirdparty/event/lib/event.lib，而看不到它由哪个 target 生成。
    target_include_directories(event INTERFACE
        ${EVENT_SRC_DIR}/include
        ${CMAKE_BINARY_DIR}/event_build/include
        ${EVENT_INSTALL_DIR}/include
    )
    add_dependencies(event libevent_install_thirdparty)

    add_library(event_pthreads INTERFACE)
    target_link_libraries(event_pthreads INTERFACE ws2_32 iphlpapi)
    add_dependencies(event_pthreads libevent_install_thirdparty)
else()
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
endif()

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

# 在添加子目录之前，确保依赖库的目录存在
# 这样可以避免 CMake 配置时检查路径不存在的错误
# 注意：这些目录会在实际构建依赖库时被填充

# 创建占位目录（如果不存在）
file(MAKE_DIRECTORY ${EVENT_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${JEMALLOC_INSTALL_DIR}/include)

# 添加子目录
add_subdirectory(tglog)
add_subdirectory(tglog-st)
add_subdirectory(test)
add_subdirectory(test-st)
if(NOT WIN32)
    add_subdirectory(test-tapp)
endif()

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
    DEPENDS jemalloc_build libevent_build
    COMMENT "Building dependencies..."
)

# 创建一个 all_deps 目标，确保所有子项目都依赖它
add_custom_target(all_deps ALL
    DEPENDS jemalloc_build libevent_build
    COMMENT "Ensuring all dependencies are built..."
)

# 让标准 clean/Clean All 也删除放在源码树 lib/ 下的 Windows 链接器工件
if(WIN32)
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
        ${LIB_DIR}/tglog-import.exp
        ${LIB_DIR}/tglog.ilk
        ${LIB_DIR}/tglog.pdb
        ${LIB_DIR}/tglog-st-import.exp
        ${LIB_DIR}/tglog-st.ilk
        ${LIB_DIR}/tglog-st.pdb
    )
endif()

# 清理依赖项
add_custom_target(clean-deps
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${JEMALLOC_INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${EVENT_INSTALL_DIR}
    COMMENT "Cleaning dependencies..."
)
if(NOT WIN32)
    add_custom_command(TARGET clean-deps POST_BUILD
        COMMAND cd ${JEMALLOC_SRC_DIR} && make clean || true
        COMMAND cd ${EVENT_SRC_DIR} && make clean || true
    )
endif()

# 检查依赖项
if(WIN32)
    add_custom_target(check-deps
        COMMAND ${CMAKE_COMMAND} -E echo "Checking dependencies..."
        COMMAND ${CMAKE_COMMAND} -E echo -n "  tglog static lib: "
        COMMAND if exist ${LIB_DIR}/tglog.lib (echo ✓ built) else (echo ✗ missing)
        COMMAND ${CMAKE_COMMAND} -E echo -n "  tglog shared lib: "
        COMMAND if exist ${LIB_DIR}/tglog.dll (echo ✓ built) else (echo ✗ missing)
        COMMAND ${CMAKE_COMMAND} -E echo -n "  Python bindings: "
        COMMAND if exist ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/tglog_client.py (echo ✓ ready) else (echo ✗ missing)
        COMMENT "Checking dependencies status..."
    )
else()
    add_custom_target(check-deps
        COMMAND ${CMAKE_COMMAND} -E echo "Checking dependencies..."
        COMMAND ${CMAKE_COMMAND} -E echo -n "  jemalloc: " 
        COMMAND test -f ${JEMALLOC_LIB} && echo "✓ built" || echo "✗ missing"
        COMMAND ${CMAKE_COMMAND} -E echo -n "  libevent: "
        COMMAND test -f ${EVENT_LIB} && echo "✓ built" || echo "✗ missing"
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
endif()

# 帮助目标
add_custom_target(help_info
    COMMAND ${CMAKE_COMMAND} -E echo "TGLog SDK Build System"
    COMMAND ${CMAKE_COMMAND} -E echo "======================"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  all          - Build everything (dependencies + TGLog + tests + bindings)"
    COMMAND ${CMAKE_COMMAND} -E echo "  deps         - Build only dependencies (jemalloc, libevent)"
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

# 安装测试程序
if(WIN32)
    # MSVC 输出到 Release/ 子目录，用 file(GLOB) 在安装时展开，直接装到 dist/bin/
    install(CODE "
        file(GLOB TEST_EXES
            \"${CMAKE_CURRENT_SOURCE_DIR}/test/Release/*.exe\"
            \"${CMAKE_CURRENT_SOURCE_DIR}/test-st/Release/*.exe\"
        )
        foreach(exe \${TEST_EXES})
            file(INSTALL \${exe} DESTINATION \"\${CMAKE_INSTALL_PREFIX}/dist/bin\")
        endforeach()
    ")
else()
    # Linux/macOS：可执行文件直接在 test/ test-st/ 目录下
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
endif()

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

# 安装第三方依赖（Windows/Linux 逻辑统一：都从 thirdparty/ 拷贝到 dist/）
if(WIN32)
    # Windows 上 jemalloc 未构建，跳过
    # libevent 构建时已通过 libevent_install_thirdparty 安装到 thirdparty/event/
    if(EXISTS ${EVENT_INSTALL_DIR}/lib)
        install(DIRECTORY ${EVENT_INSTALL_DIR}/lib/
            DESTINATION dist/thirdparty/event/lib
            FILES_MATCHING PATTERN "*.lib"
        )
    endif()
    if(EXISTS ${EVENT_INSTALL_DIR}/include)
        install(DIRECTORY ${EVENT_INSTALL_DIR}/include/
            DESTINATION dist/thirdparty/event/include
        )
    endif()
else()
    # jemalloc
    if(EXISTS ${JEMALLOC_INSTALL_DIR}/lib)
        install(DIRECTORY ${JEMALLOC_INSTALL_DIR}/lib/
            DESTINATION dist/thirdparty/jemalloc/lib
        )
        install(DIRECTORY ${JEMALLOC_INSTALL_DIR}/include/
            DESTINATION dist/thirdparty/jemalloc/include
        )
    endif()

    # libevent
    if(EXISTS ${EVENT_INSTALL_DIR}/lib)
        install(DIRECTORY ${EVENT_INSTALL_DIR}/lib/
            DESTINATION dist/thirdparty/event/lib
        )
        install(DIRECTORY ${EVENT_INSTALL_DIR}/include/
            DESTINATION dist/thirdparty/event/include
        )
    endif()
endif()

# 创建pkg-config文件
if(NOT WIN32)
# TGLog (multi-threaded version)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/tglog.pc"
"prefix=${CMAKE_CURRENT_SOURCE_DIR}/dist
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: TGLog
Description: High-performance logging library (multi-threaded)
Version: 1.0.0
Libs: -L\${libdir} -ltglog -L\${prefix}/thirdparty/event/lib -levent -levent_pthreads -L\${prefix}/thirdparty/jemalloc/lib -ljemalloc -lpthread -latomic
Cflags: -I\${includedir}/tglog -I\${prefix}/thirdparty/event/include
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
Libs: -L\${libdir} -ltglog-st -L\${prefix}/thirdparty/event/lib -levent -L\${prefix}/thirdparty/jemalloc/lib -ljemalloc -lpthread
Cflags: -I\${includedir}/tglog-st -I\${prefix}/thirdparty/event/include
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tglog-st.pc
    DESTINATION dist/lib/pkgconfig
)
endif()

# 创建README文件
install(CODE "
    file(WRITE \${CMAKE_INSTALL_PREFIX}/dist/README.md \"# TGLog SDK Distribution

## Directory Structure

```
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
    └── jemalloc/           # jemalloc
```

## Quick Start

### 1. Using Multi-threaded Version (tglog)

```c
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
```

### 2. Using Single-threaded Version (tglog-st)

```c
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
```

## Compilation Examples

### Method 1: Direct Compilation (Recommended)

```bash
# Multi-threaded version
gcc -o myapp myapp.c \\\\
    -Iinclude/tglog \\\\
    -Ithirdparty/event/include \\\\
    -Llib -Lthirdparty/event/lib -Lthirdparty/jemalloc/lib \\\\
    -ltglog -levent -levent_pthreads -ljemalloc -lpthread -latomic

# Single-threaded version
gcc -o myapp myapp.c \\\\
    -Iinclude/tglog-st \\\\
    -Ithirdparty/event/include \\\\
    -Llib -Lthirdparty/event/lib -Lthirdparty/jemalloc/lib \\\\
    -ltglog-st -levent -ljemalloc -lpthread -latomic
```

### Method 2: Using pkg-config (Easiest)

pkg-config automatically includes all dependencies (libevent, jemalloc):

```bash
# Set PKG_CONFIG_PATH (relative to dist directory)
export PKG_CONFIG_PATH=\\\$(pwd)/lib/pkgconfig:\\\$PKG_CONFIG_PATH

# Multi-threaded version
gcc -o myapp myapp.c `pkg-config --cflags --libs tglog`

# Single-threaded version
gcc -o myapp myapp.c `pkg-config --cflags --libs tglog-st`
```

### Method 3: Using Makefile (Like test/ examples)

```makefile
# Multi-threaded version
LIBDIR := lib thirdparty/event/lib thirdparty/jemalloc/lib
STATIC_LIBS := tglog event event_pthreads jemalloc
DYNAMIC_LIBS := pthread
INCLUDES := include/tglog thirdparty/event/include

myapp: myapp.c
	gcc \\\$(addprefix -I,\\\$(INCLUDES)) -o \\\$@ \\\$< \\\\
	    \\\$(addprefix -L,\\\$(LIBDIR)) \\\\
	    \\\$(addprefix -l,\\\$(STATIC_LIBS)) \\\\
	    \\\$(addprefix -l,\\\$(DYNAMIC_LIBS))

# Single-threaded version: replace tglog with tglog-st, include/tglog with include/tglog-st
```

## Key Differences

| Feature | Multi-threaded (tglog) | Single-threaded (tglog-st) |
|---------|------------------------|----------------------------|
| Performance | ~1M logs/sec | ~100K-200K logs/sec |
| Worker Thread | Yes (automatic) | No (manual update) |
| Update Call | Not needed | **Required** `tglog_client_update()` |
| Memory Usage | Higher | Lower |
| Use Case | High-throughput servers | Single-threaded apps, embedded |

## Examples

### Test Programs

See `bin/` directory for basic test programs:
- Multi-threaded: `tglog_test`
- Single-threaded: `tglog_test_st`, `simple_perf_test_st`

Run test programs:
```bash
cd bin
./tglog_test           # Multi-threaded test
./tglog_test_st        # Single-threaded test
./simple_perf_test_st  # Single-threaded performance test
```

### TAPP Integration Example

See `examples/test-tapp/` for a complete TAPP application example:

```bash
cd examples/test-tapp
./start.sh    # Start the TAPP application
./stop.sh     # Stop the TAPP application
```

## Language Bindings

- **Python**: See `bindings/python/README_PYTHON.md`

## Documentation

For detailed documentation, see the source repository:
- `tglog/DESIGN.md` - Multi-threaded version design
- `tglog-st/DESIGN.md` - Single-threaded version design
\")
")