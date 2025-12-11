add_library(scanner
        src/Lexer.cpp
)

target_include_directories(scanner PUBLIC include)

target_link_libraries(scanner PUBLIC common)