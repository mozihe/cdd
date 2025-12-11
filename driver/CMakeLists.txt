add_executable(cdd
        main.cpp
)

# 包含生成的版本头文件
target_include_directories(cdd PRIVATE
    ${CMAKE_SOURCE_DIR}/codegen/include
    ${CMAKE_SOURCE_DIR}/preprocessor/include
    ${CMAKE_SOURCE_DIR}/parser/include
    ${CMAKE_SOURCE_DIR}/scanner/include
    ${CMAKE_SOURCE_DIR}/semantic/include
    ${CMAKE_SOURCE_DIR}/codegen/include
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(cdd preprocessor parser scanner common semantic codegen)

# 安装可执行文件
install(TARGETS cdd
    RUNTIME DESTINATION ${CDD_INSTALL_BINDIR}
)

# 创建符号链接 cdd-cc (用于类 gcc 的调用方式)
install(CODE "
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink cdd cdd-cc
        WORKING_DIRECTORY \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CDD_INSTALL_BINDIR}
    )
")
