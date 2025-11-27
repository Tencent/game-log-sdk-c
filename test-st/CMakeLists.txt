# test-st 程序的 CMakeLists.txt

# 收集所有测试源文件
file(GLOB TEST_SOURCES "*.c")

# 设置库目录
set(LIBDIRS
        ${CMAKE_SOURCE_DIR}/lib
        ${CMAKE_SOURCE_DIR}/thirdparty/event/lib
        ${CMAKE_SOURCE_DIR}/thirdparty/ck/lib
        ${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/lib
)

# 设置包含目录
set(INCLUDES
        .
        ${CMAKE_SOURCE_DIR}/tglog-st
        ${CMAKE_SOURCE_DIR}/thirdparty/event/include
        ${CMAKE_SOURCE_DIR}/thirdparty/ck/include
)

# 设置编译选项
if (APPLE)
    set(TEST_CFLAGS -Wall -Werror -Wno-typedef-redefinition -O0 -g -std=c99 -D_GNU_SOURCE)
else ()
    set(TEST_CFLAGS -Wall -Werror -O0 -g -std=c99 -D_GNU_SOURCE -D_REENTRANT)
endif ()

# 为每个 .c 文件创建一个可执行文件
foreach (test_source ${TEST_SOURCES})
    # 获取文件名（不含扩展名）
    get_filename_component(test_name ${test_source} NAME_WE)

    # 添加 _st 后缀以区分多线程版本
    set(test_exe_name ${test_name}_st)

    # 创建可执行文件
    add_executable(${test_exe_name} ${test_source})

    # 设置编译选项
    target_compile_options(${test_exe_name} PRIVATE ${TEST_CFLAGS})

    # 设置包含目录
    target_include_directories(${test_exe_name} PRIVATE ${INCLUDES})

    # 设置链接目录
    target_link_directories(${test_exe_name} PRIVATE ${LIBDIRS})

    # 所有平台都使用静态链接
    if (APPLE)
        target_link_libraries(${test_exe_name} PRIVATE
                tglog-st_static event ck jemalloc
                m z pthread
        )
    else ()
        # Linux 系统
        target_link_options(${test_exe_name} PRIVATE -Wl,-dn)
        target_link_libraries(${test_exe_name} PRIVATE
                tglog-st_static event ck jemalloc
        )
        target_link_options(${test_exe_name} PRIVATE -Wl,-dy)
        target_link_libraries(${test_exe_name} PRIVATE
                m z rt dl pthread
        )
    endif ()

    # 确保依赖项已经构建
    add_dependencies(${test_exe_name} tglog-st_static jemalloc event ck)

    # 设置输出目录为当前目录
    set_target_properties(${test_exe_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endforeach ()

# 创建一个总的测试目标
add_custom_target(test_st_programs ALL)

# 为所有测试程序添加依赖
foreach (test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_exe_name ${test_name}_st)
    add_dependencies(test_st_programs ${test_exe_name})
endforeach ()

# 清理目标
add_custom_target(test_st_clean
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/*_st
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Cleaning test-st executables..."
)