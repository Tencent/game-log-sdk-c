# CMakeLists.txt for TgLog Language Bindings

# 添加子目录
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/python/CMakeLists.txt)
    add_subdirectory(python)
else()
    if(WIN32)
        # 如果没有 CMakeLists.txt，使用自定义命令
        add_custom_target(python_bindings
            COMMAND python setup_ext.py build_ext --inplace
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
            COMMENT "Building Python bindings..."
            DEPENDS tglog_static tglog_shared
        )
    else()
        # 如果没有 CMakeLists.txt，使用自定义命令
        add_custom_target(python_bindings
            COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/python all
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
            COMMENT "Building Python bindings..."
            DEPENDS tglog_static tglog_shared
        )
    endif()
endif()

# 创建总的 bindings 目标（设置为 ALL，默认构建）
add_custom_target(bindings ALL)
add_dependencies(bindings python_bindings tglog_static tglog_shared)

# Help target
add_custom_target(bindings_help
    COMMAND ${CMAKE_COMMAND} -E echo "TgLog Language Bindings"
    COMMAND ${CMAKE_COMMAND} -E echo "======================="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  bindings             - Build all bindings (Python)"
    COMMAND ${CMAKE_COMMAND} -E echo "  python_bindings      - Build Python bindings"
    COMMENT "Showing bindings help..."
)
