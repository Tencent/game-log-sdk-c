# test 程序的 CMakeLists.txt

# 收集所有测试源文件
file(GLOB TEST_SOURCES "*.c")

# 设置库目录
set(LIBDIRS
        ${CMAKE_SOURCE_DIR}/lib
        ${CMAKE_SOURCE_DIR}/thirdparty/event/lib
)
if(NOT WIN32)
    list(APPEND LIBDIRS ${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/lib)
endif()
if(WIN32)
    list(APPEND LIBDIRS ${CMAKE_SOURCE_DIR}/lib/Release)
endif()

# 设置包含目录
set(INCLUDES
        .
        ${CMAKE_SOURCE_DIR}/tglog
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_SOURCE_DIR}/thirdparty/event/include
)

# 设置编译选项
if(MSVC)
    set(TEST_CFLAGS /W3 /D_CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    set(TEST_CFLAGS -Wall -Werror -Wno-typedef-redefinition -O0 -g -std=c99 -D_GNU_SOURCE)
else()
    set(TEST_CFLAGS -Wall -Werror -O0 -g -std=c99 -D_GNU_SOURCE -D_REENTRANT -mcx16)
endif()

# 为每个 .c 文件创建一个可执行文件
foreach (test_source ${TEST_SOURCES})
    # 获取文件名（不含扩展名）
    get_filename_component(test_name ${test_source} NAME_WE)

    # 创建可执行文件
    add_executable(${test_name} ${test_source})

    # 设置编译选项
    target_compile_options(${test_name} PRIVATE ${TEST_CFLAGS})

    # 设置包含目录
    target_include_directories(${test_name} PRIVATE ${INCLUDES})

    # 设置链接目录
    target_link_directories(${test_name} PRIVATE ${LIBDIRS})

    if(MSVC)
        # Windows/MSVC 链接：与 Linux 统一使用 event + event_pthreads
        target_link_libraries(${test_name} PRIVATE
                tglog_static
                event event_pthreads
        )
        add_dependencies(${test_name} tglog_static libevent_build)
    elseif(APPLE)
        target_link_libraries(${test_name} PRIVATE
                tglog_static event event_pthreads jemalloc
                m z pthread
        )
        add_dependencies(${test_name} tglog_static jemalloc event event_pthreads)
    else()
        # Linux 系统
        target_link_options(${test_name} PRIVATE -Wl,-dn)
        target_link_libraries(${test_name} PRIVATE
                tglog_static event event_pthreads jemalloc
        )
        target_link_options(${test_name} PRIVATE -Wl,-dy)
        target_link_libraries(${test_name} PRIVATE
                m z rt dl pthread
        )
        add_dependencies(${test_name} tglog_static jemalloc event event_pthreads)
    endif()

    # 设置输出目录为当前目录
    set_target_properties(${test_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(WIN32)
        set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.exp
                ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.ilk
                ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.pdb
        )
    endif()
endforeach ()

# 创建一个总的测试目标
if(WIN32)
    add_custom_target(test_programs)
else()
    add_custom_target(test_programs ALL DEPENDS ${TEST_SOURCES})
endif()

# 为所有测试程序添加依赖
foreach (test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_dependencies(test_programs ${test_name})
endforeach ()

# 清理目标
add_custom_target(test_clean
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/*[^.c]
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Cleaning test executables..."
)
