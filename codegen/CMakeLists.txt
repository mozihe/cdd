add_library(codegen
    src/CodeGenerator.cpp
)

target_include_directories(codegen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/semantic/include
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/scanner/include
)

target_link_libraries(codegen
    semantic
    common
)
