add_library(semantic
        src/Type.cpp
        src/SymbolTable.cpp
        src/SemanticAnalyzer.cpp
        src/IRGenerator.cpp
)

target_include_directories(semantic PUBLIC include)
target_include_directories(semantic PUBLIC ${CMAKE_SOURCE_DIR}/common/include)
target_link_libraries(semantic PUBLIC common)
