cmake_minimum_required(VERSION 3.10)
project(tglog-st C)

set(CMAKE_C_STANDARD 99)

# 设置编译器标志
# 注意：优化级别 (-O2/-O0) 和调试符号 (-g) 由根目录的 CMAKE_C_FLAGS 控制
if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fPIC -std=c99 -D_GNU_SOURCE")
endif()

# 包含目录 - 使用变量存储，但不使用全局include_directories
set(TGLOG_ST_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_SOURCE_DIR}/thirdparty/event/include
        ${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/include
)

# 源文件
set(SOURCES
        client.c
        conn.c
        endpoint.c
        request.c
        conf.c
        err.c
        metrics.c
        debug_logger.c
        worker.c
        discoverer.c
        pool.c
        ${CMAKE_SOURCE_DIR}/common/tglog_ht.c
)

# 头文件
set(HEADERS
        client.h
        conn.h
        endpoint.h
        request.h
        conf.h
        err.h
        def.h
        metrics.h
        debug_logger.h
        worker.h
        discoverer.h
        pool.h
        tglog.h
)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# 创建静态库
add_library(tglog-st_static STATIC ${SOURCES})
set_target_properties(tglog-st_static PROPERTIES OUTPUT_NAME "tglog-st")

# 设置静态库的包含目录
target_include_directories(tglog-st_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/event/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/event/include>  # 添加源码目录供IDE使用
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/include>
        $<INSTALL_INTERFACE:include/tglog-st>
)
if(WIN32)
    target_include_directories(tglog-st_static PUBLIC
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/event/include>
    )
endif()

# 创建动态库
add_library(tglog-st_shared SHARED ${SOURCES})
set_target_properties(tglog-st_shared PROPERTIES OUTPUT_NAME "tglog-st")

# 设置动态库的包含目录
target_include_directories(tglog-st_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/event/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/event/include>  # 添加源码目录供IDE使用
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/include>
        $<INSTALL_INTERFACE:include/tglog-st>
)
if(WIN32)
    target_include_directories(tglog-st_shared PUBLIC
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/event/include>
    )
endif()

# 链接依赖库
target_link_directories(tglog-st_static PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/event/lib
        ${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/lib
)

target_link_directories(tglog-st_shared PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/event/lib
        ${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/lib
)

# 根据系统平台设置链接选项
# Note: jemalloc is NOT linked here - it should be linked by the final executable
if (APPLE)
    target_link_libraries(tglog-st_shared PRIVATE
            event
            pthread m
    )
elseif (WIN32)
    target_link_libraries(tglog-st_shared PRIVATE
            event_core_static
            ws2_32
            iphlpapi
    )
else ()
    # Linux 系统，使用 -Wl,-Bstatic 和 -Wl,-Bdynamic
    target_link_options(tglog-st_shared PRIVATE
            -Wl,-Bstatic
    )
    target_link_libraries(tglog-st_shared PRIVATE
            event
    )
    target_link_options(tglog-st_shared PRIVATE
            -Wl,-Bdynamic
    )
    target_link_libraries(tglog-st_shared PRIVATE
            pthread m
    )
endif ()

# 添加依赖
add_dependencies(tglog-st_static jemalloc_build libevent_build event)
add_dependencies(tglog-st_shared jemalloc_build libevent_build event)

# 测试程序已移至 test-st 目录

# 安装规则
install(TARGETS tglog-st_static tglog-st_shared
        ARCHIVE DESTINATION dist/lib
        LIBRARY DESTINATION dist/lib
        RUNTIME DESTINATION dist/bin
)

install(FILES ${HEADERS}
        DESTINATION dist/include/tglog-st
)

# 安装 common 头文件
file(GLOB TGLOG_ST_COMMON_HEADERS "${CMAKE_SOURCE_DIR}/common/*.h")
install(FILES ${TGLOG_ST_COMMON_HEADERS}
        DESTINATION dist/include/tglog-st/common
)

# 创建接口库用于IDE支持
add_library(tglog-st INTERFACE)
target_include_directories(tglog-st INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/event/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/event/include>  # 添加源码目录供IDE使用
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/event>  # 添加event根目录
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/include>
)
target_link_libraries(tglog-st INTERFACE tglog-st_static)

# 自定义目标
add_custom_target(tglog-st_all ALL DEPENDS tglog-st_static tglog-st_shared)