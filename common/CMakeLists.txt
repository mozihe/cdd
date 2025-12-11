add_library(common
        include/TokenKind.h
        include/SourceLocation.h
        include/Ast.h
)

# === [新增] 强制指定链接语言为 C++ ===
set_target_properties(common PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(common PUBLIC include)