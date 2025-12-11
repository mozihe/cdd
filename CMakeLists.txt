cmake_minimum_required(VERSION 3.10)
project(Cdd 
    VERSION 1.0.0
    DESCRIPTION "CDD C Compiler - A simple C compiler for educational purposes"
    LANGUAGES C CXX
)

# ============================================================================
# 构建选项
# ============================================================================
option(CDD_BUILD_TESTS "Build test suite" ON)
option(CDD_INSTALL_STDLIB "Install standard library headers" ON)
option(CDD_ENABLE_DEBUG "Enable debug output" OFF)

# ============================================================================
# 编译器设置
# ============================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 调试模式额外标志
if(CDD_ENABLE_DEBUG)
    add_compile_definitions(CDD_DEBUG)
endif()

# 编译警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# 版本信息配置
# ============================================================================
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/version.h"
    @ONLY
)

# ============================================================================
# 安装路径配置
# ============================================================================
include(GNUInstallDirs)

set(CDD_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
set(CDD_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/cdd")
set(CDD_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/cdd")
set(CDD_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/cdd")

# ============================================================================
# 添加子模块
# ============================================================================
add_subdirectory(common)
add_subdirectory(preprocessor)
add_subdirectory(scanner)
add_subdirectory(parser)
add_subdirectory(semantic)
add_subdirectory(codegen)
add_subdirectory(driver)

# ============================================================================
# CDD 运行时库 (libcdd.so)
# ============================================================================
if(CDD_INSTALL_STDLIB)
    # 编译 CDD 运行时库
    add_library(cddrt SHARED stdlib/cddlib.c)
    set_target_properties(cddrt PROPERTIES
        OUTPUT_NAME "cdd"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    # 禁用内置函数优化，防止 memset/memcpy 等函数递归调用自己
    target_compile_options(cddrt PRIVATE -fno-builtin)
    
    # 安装运行时库
    install(TARGETS cddrt
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
    
    # 安装头文件
    install(DIRECTORY stdlib/
        DESTINATION "${CDD_INSTALL_INCLUDEDIR}"
        FILES_MATCHING 
        PATTERN "*.hdd"
    )
endif()

# ============================================================================
# 安装文档
# ============================================================================
install(FILES README.md LICENSE DESTINATION "${CDD_INSTALL_DATADIR}" OPTIONAL)

# ============================================================================
# 卸载目标
# ============================================================================
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()

# ============================================================================
# 打包配置 (CPack)
# ============================================================================
set(CPACK_PACKAGE_NAME "cdd")
set(CPACK_PACKAGE_VENDOR "CDD Compiler Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Debian 包配置
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "CDD Team")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libstdc++6 (>= 4.8)")

include(CPack)

# ============================================================================
# 构建摘要
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  CDD Compiler v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "  Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build tests:       ${CDD_BUILD_TESTS}")
message(STATUS "  Install stdlib:    ${CDD_INSTALL_STDLIB}")
message(STATUS "  Debug mode:        ${CDD_ENABLE_DEBUG}")
message(STATUS "========================================")
message(STATUS "")
