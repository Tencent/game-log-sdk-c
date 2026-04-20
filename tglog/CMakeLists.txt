# tglog 库的 CMakeLists.txt

# 设置输出路径
set(EXECUTABLE ${CMAKE_SOURCE_DIR}/lib/libtglog.a)
set(EXECUTABLE2 ${CMAKE_SOURCE_DIR}/lib/libtglog.so)

# 收集所有源文件
file(GLOB SRCS "*.c")
list(APPEND SRCS "${CMAKE_SOURCE_DIR}/common/tglog_ht.c")
list(APPEND SRCS "${CMAKE_SOURCE_DIR}/common/tglog_platform.c")

# 创建对象文件
add_library(tglog_objs OBJECT ${SRCS})

# 设置编译选项
if(MSVC)
    target_compile_options(tglog_objs PRIVATE
            /W3
    )
else()
    target_compile_options(tglog_objs PRIVATE
            -Wall -Werror -std=c99
            -D_GNU_SOURCE -D_REENTRANT
            -Wno-typedef-redefinition
            -fPIC -m64 -pipe -mcx16
    )
endif()

# 优化选项由根目录的 CMAKE_C_FLAGS 控制
# 如果需要覆盖，可以在这里添加特定的优化选项

# 设置包含目录
target_include_directories(tglog_objs PRIVATE
        .
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_SOURCE_DIR}/thirdparty/event/include
)

# 确保依赖项已经构建
add_dependencies(tglog_objs jemalloc_build libevent_build event)

# 创建静态库
add_library(tglog_static STATIC $<TARGET_OBJECTS:tglog_objs>)
set_target_properties(tglog_static PROPERTIES
        OUTPUT_NAME "tglog"
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
        PDB_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

# 为静态库设置依赖
add_dependencies(tglog_static tglog_objs)

# 创建动态库（仅在 BUILD_SHARED 定义时）
if (BUILD_SHARED)
    add_library(tglog_shared SHARED $<TARGET_OBJECTS:tglog_objs>)
    set_target_properties(tglog_shared PROPERTIES
            OUTPUT_NAME "tglog"
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
            PDB_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
    )
    if(WIN32)
        set_target_properties(tglog_shared PROPERTIES
                ARCHIVE_OUTPUT_NAME "tglog-import"
                LINK_FLAGS "/DEBUG"
        )
    endif()

    # 链接动态库需要的库
    target_link_directories(tglog_shared PRIVATE
            ${CMAKE_SOURCE_DIR}/thirdparty/event/lib
    )
    if(NOT WIN32)
        target_link_directories(tglog_shared PRIVATE
                ${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/lib
        )
    endif()

    # 根据系统平台设置链接选项
    if (APPLE)
        target_link_libraries(tglog_shared PRIVATE
                event event_pthreads
                m z dl pthread
        )
    elseif (WIN32)
        target_link_libraries(tglog_shared PRIVATE
                event event_pthreads
        )
    else ()
        # Linux 系统，使用 -Wl,-Bstatic 和 -Wl,-Bdynamic
        # Note: jemalloc is NOT linked here - it should be linked by the final executable
        target_link_options(tglog_shared PRIVATE
                -Wl,-Bstatic
        )
        target_link_libraries(tglog_shared PRIVATE
                event event_pthreads
        )
        target_link_options(tglog_shared PRIVATE
                -Wl,-Bdynamic
        )
        target_link_libraries(tglog_shared PRIVATE
                m z rt dl pthread
        )
    endif ()

    add_dependencies(tglog_shared tglog_objs)
endif ()

# 创建一个总的 tglog 目标
add_custom_target(tglog ALL)
add_dependencies(tglog tglog_static)
if (BUILD_SHARED)
    add_dependencies(tglog tglog_shared)
endif ()

# 安装规则
install(TARGETS tglog_static
        ARCHIVE DESTINATION dist/lib
        LIBRARY DESTINATION dist/lib
        RUNTIME DESTINATION dist/lib
)
if (BUILD_SHARED)
    install(TARGETS tglog_shared
            ARCHIVE DESTINATION dist/lib
            LIBRARY DESTINATION dist/lib
            RUNTIME DESTINATION dist/lib
    )
    if(WIN32)
        # 安装 PDB 和 EXP 文件到 dist/lib
        install(FILES
                $<TARGET_PDB_FILE:tglog_shared>
                "$<TARGET_LINKER_FILE_DIR:tglog_shared>/tglog-import.exp"
                DESTINATION dist/lib
                OPTIONAL
        )
    endif()
endif ()

# 安装头文件
file(GLOB TGLOG_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
install(FILES ${TGLOG_HEADERS} DESTINATION dist/include/tglog)

# 安装 common 头文件
file(GLOB TGLOG_COMMON_HEADERS "${CMAKE_SOURCE_DIR}/common/*.h")
install(FILES ${TGLOG_COMMON_HEADERS} DESTINATION dist/include/tglog/common)

# 信息目标
add_custom_target(tglog_info
        COMMAND ${CMAKE_COMMAND} -E echo "Source files: ${SRCS}"
        COMMENT "Showing tglog build information..."
)