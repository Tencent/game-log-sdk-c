# test-tapp 程序的 CMakeLists.txt

# 设置TSF4G路径
set(TSF4G_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tsf4g)
set(TDR_TOOL ${TSF4G_DIR}/tools/tdr)

# TDR 生成的文件
set(TDR_GENERATED_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_def.tdr
    ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_def_t.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_def.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_def.h
)

# 如果 TDR 工具存在且生成的文件不存在，则生成它们
if(EXISTS ${TDR_TOOL} AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_def.h)
    message(STATUS "Generating TDR files for test-tapp...")
    execute_process(
        COMMAND ${TDR_TOOL} -B tglog_tapp_def.xml -o tglog_tapp_def.tdr
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    execute_process(
        COMMAND ${TDR_TOOL} -G -m TestTappCfg tglog_tapp_def.xml -o tglog_tapp_def_t.xml
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    execute_process(
        COMMAND ${TDR_TOOL} -C tglog_tapp_def.xml -o tglog_tapp_def.c
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    execute_process(
        COMMAND ${TDR_TOOL} -H tglog_tapp_def.xml
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# 收集所有源文件
file(GLOB TEST_SOURCES
    "tglog_tapp.c"
    "tglog_tapp_def.c"
)

# 设置库目录
set(LIBDIRS
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/event/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/ck/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/jemalloc/lib
    ${TSF4G_DIR}/lib
)

# 设置包含目录
set(INCLUDES
    .
    ${CMAKE_SOURCE_DIR}/tglog-st
    ${CMAKE_SOURCE_DIR}/thirdparty/event/include
    ${CMAKE_SOURCE_DIR}/thirdparty/ck/include
    ${TSF4G_DIR}/include
)

# 设置编译选项
if (APPLE)
    set(TEST_CFLAGS -Wall -Wno-typedef-redefinition -O0 -g -std=c99 -D_GNU_SOURCE)
else ()
    set(TEST_CFLAGS -Wall -O0 -g -std=c99 -D_GNU_SOURCE -D_REENTRANT)
endif ()

# 创建可执行文件
add_executable(tglog_tapp ${TEST_SOURCES})

# 设置编译选项
target_compile_options(tglog_tapp PRIVATE ${TEST_CFLAGS})

# 设置包含目录
target_include_directories(tglog_tapp PRIVATE ${INCLUDES})

# 设置链接目录
target_link_directories(tglog_tapp PRIVATE ${LIBDIRS})

# 链接库
if (APPLE)
    target_link_libraries(tglog_tapp PRIVATE
        tglog-st_static event ck jemalloc
        tapp readline ncurses tsf4g scew expat
        m z pthread
    )
else ()
    # Linux 系统
    target_link_options(tglog_tapp PRIVATE -Wl,-dn)
    target_link_libraries(tglog_tapp PRIVATE
        tglog-st_static event ck jemalloc
        tapp readline ncurses tsf4g scew expat
    )
    target_link_options(tglog_tapp PRIVATE -Wl,-dy)
    target_link_libraries(tglog_tapp PRIVATE
        m z rt dl pthread anl stdc++
    )
endif ()

# 确保依赖项已经构建
add_dependencies(tglog_tapp tglog-st_static jemalloc event ck)

# 设置输出目录为当前目录
set_target_properties(tglog_tapp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# 创建用于生成TDR文件的自定义目标（可选，如果需要重新生成）
add_custom_target(tglog_tapp_tdr
    COMMAND ${TSF4G_DIR}/tools/tdr -B tglog_tapp_def.xml -o tglog_tapp_def.tdr
    COMMAND ${TSF4G_DIR}/tools/tdr -G -m TestTappCfg tglog_tapp_def.xml -o tglog_tapp_def_t.xml
    COMMAND ${TSF4G_DIR}/tools/tdr -C tglog_tapp_def.xml -o tglog_tapp_def.c
    COMMAND ${TSF4G_DIR}/tools/tdr -H tglog_tapp_def.xml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Regenerating TDR configuration files from XML"
)

# 复制配置文件到构建目录（如果需要）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_cfg.xml)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_cfg.xml
        ${CMAKE_CURRENT_BINARY_DIR}/tglog_tapp_cfg.xml COPYONLY)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tlog_cfg.xml)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tlog_cfg.xml
        ${CMAKE_CURRENT_BINARY_DIR}/tlog_cfg.xml COPYONLY)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_def.tdr)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp_def.tdr
        ${CMAKE_CURRENT_BINARY_DIR}/tglog_tapp_def.tdr COPYONLY)
endif()

# 复制脚本文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/start.sh)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/start.sh
        ${CMAKE_CURRENT_BINARY_DIR}/start.sh COPYONLY)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stop.sh)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/stop.sh
        ${CMAKE_CURRENT_BINARY_DIR}/stop.sh COPYONLY)
endif()

# 清理目标
add_custom_target(test_tapp_clean
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_SOURCE_DIR}/tglog_tapp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Cleaning test-tapp executables..."
)