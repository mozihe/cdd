/**
 * @file main.cpp
 * @brief CDD 编译器驱动程序
 * @author mozihe
 * 
 * 编译器入口，协调预处理、词法分析、语法分析、语义分析、
 * 中间代码生成和目标代码生成各阶段。
 */

#include <iostream>
#include <fstream>
#include <vector>
#include "Preprocessor.h"
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "IRGenerator.h"
#include "CodeGenerator.h"
#include "Debug.h"

using namespace cdd;

void printUsage(const char* prog) {
    std::cerr << "CDD Compiler v1.0\n";
    std::cerr << "Usage: " << prog << " [options] <source_file.cdd>\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -p, --preprocess    仅预处理\n";
    std::cerr << "  -l, --lex           仅词法分析\n";
    std::cerr << "  -a, --ast           仅生成 AST\n";
    std::cerr << "  -s, --semantic      仅语义分析\n";
    std::cerr << "  -i, --ir            生成中间代码（四元式）\n";
    std::cerr << "  -S, --asm           生成汇编代码\n";
    std::cerr << "  -c, --compile       编译为可执行文件\n";
    std::cerr << "  -o <file>           输出文件名\n";
    std::cerr << "  -I <path>           添加头文件搜索路径\n";
    std::cerr << "  -h, --help          显示帮助信息\n";
    std::cerr << "Supported file extensions: .cdd, .c\n\n";
    std::cerr << "Environment Variables:\n";
    std::cerr << "  CDD_INCLUDE_PATH    头文件搜索路径\n";
#ifdef CDD_DEBUG
    std::cerr << "\n[Built with DEBUG mode enabled]\n";
#endif
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    // Parse command line arguments
    bool onlyPreprocess = false;
    bool onlyLex = false;
    bool onlyAst = false;
    bool doSemantic = false;
    bool doIR = false;
    bool doAsm = false;
    bool doCompile = false;
    std::string filename;
    std::string outputFile;
    std::vector<std::string> extraIncludePaths;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--preprocess") onlyPreprocess = true;
        else if (arg == "-l" || arg == "--lex") onlyLex = true;
        else if (arg == "-a" || arg == "--ast") onlyAst = true;
        else if (arg == "-s" || arg == "--semantic") doSemantic = true;
        else if (arg == "-i" || arg == "--ir") doIR = true;
        else if (arg == "-S" || arg == "--asm") doAsm = true;
        else if (arg == "-c" || arg == "--compile") doCompile = true;
        else if (arg == "-o" && i + 1 < argc) outputFile = argv[++i];
        else if (arg == "-I" && i + 1 < argc) extraIncludePaths.push_back(argv[++i]);
        else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg[0] != '-') filename = arg;
    }
    
    if (filename.empty()) {
        std::cerr << "Error: No source file specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    // 默认输出文件名
    if (outputFile.empty()) {
        size_t dot = filename.rfind('.');
        std::string base = (dot != std::string::npos) ? filename.substr(0, dot) : filename;
        if (doCompile) outputFile = base;
        else outputFile = base + ".s";
    }

    try {
        // ====================================================================
        // 编译流程：预处理 -> 词法分析 -> 语法分析 -> 语义分析 -> IR -> 代码生成
        // ====================================================================
        
        // 阶段 1：预处理
        // - 处理 #include, #define, #ifdef 等指令
        // - 展开宏
        // - 输出纯 C 代码
        CDD_DBG_STAGE("Preprocessing");
        CDD_DBG("Input file: " << filename);
        
        Preprocessor pp;
        for (const auto& path : extraIncludePaths) {
            pp.addIncludePath(path);
            CDD_DBG("Added include path: " << path);
        }
        std::string processedCode = pp.preprocess(filename);
        CDD_DBG("Preprocessed code size: " << processedCode.size() << " bytes");
        
        if (onlyPreprocess) {
            std::cout << "=== Preprocessed Code ===\n" << processedCode << "\n";
            return 0;
        }

        // 阶段 2：词法分析
        // - 将字符流转换为 Token 流
        CDD_DBG_STAGE("Lexical Analysis");
        Lexer lexer(processedCode, filename);
        CDD_DBG("Lexer initialized");
        
        if (onlyLex) {
            std::cout << "=== Tokens ===\n";
            // 输出所有 token
            Token tok;
            do {
                tok = lexer.nextToken();
                std::cout << "[" << tok.location.line << ":" << tok.location.column 
                          << "] " << tokenKindName(tok.kind);
                if (tok.kind == TokenKind::Identifier) {
                    std::cout << " '" << tok.stringValue << "'";
                } else if (tok.kind == TokenKind::IntLiteral) {
                    std::cout << " " << tok.intValue;
                } else if (tok.kind == TokenKind::FloatLiteral) {
                    std::cout << " " << tok.floatValue;
                } else if (tok.kind == TokenKind::StringLiteral) {
                    std::cout << " \"" << tok.stringValue << "\"";
                } else if (tok.kind == TokenKind::CharLiteral) {
                    std::cout << " '" << tok.charValue << "'";
                } else if (tok.kind == TokenKind::Invalid) {
                    std::cout << " [错误: " << tok.stringValue << "]";
                }
                std::cout << "\n";
            } while (!tok.isEof());
            
            // 输出词法分析错误
            if (lexer.hasErrors()) {
                std::cerr << "\n=== 词法分析错误 ===\n";
                for (const auto& err : lexer.errors()) {
                    std::cerr << "第 " << err.location.line << " 行，第 " 
                              << err.location.column << " 列: " << err.message << "\n";
                }
                return 1;
            }
            return 0;
        }

        // 阶段 3：语法分析
        // - 递归下降解析
        // - 构建抽象语法树（AST）
        CDD_DBG_STAGE("Parsing");
        Lexer parserLexer(processedCode, filename);
        Parser parser(parserLexer);
        auto ast = parser.parseTranslationUnit();
        
        // 检查词法分析错误
        if (parserLexer.hasErrors()) {
            std::cerr << "错误: 词法分析失败\n";
            for (const auto& err : parserLexer.errors()) {
                std::cerr << "  第 " << err.location.line << " 行，第 " 
                          << err.location.column << " 列: " << err.message << "\n";
            }
            return 1;
        }
        
        if (!ast || parser.hasErrors()) {
            std::cerr << "错误: 语法分析失败\n";
            for (const auto& err : parser.errors()) {
                std::cerr << "  " << err.what() << "\n";
            }
            return 1;
        }
        CDD_DBG("AST constructed successfully");
        
        if (onlyAst) {
            std::cout << "=== Abstract Syntax Tree ===\n";
            ast::AstPrinter printer(std::cout);
            printer.print(ast.get());
            return 0;
        }
        
        // 阶段 4：语义分析
        // - 类型检查
        // - 符号解析和作用域管理
        // - 语义约束验证
        CDD_DBG_STAGE("Semantic Analysis");
        semantic::SemanticAnalyzer analyzer;
        bool semanticOK = analyzer.analyze(ast.get());
        CDD_DBG("Semantic analysis result: " << (semanticOK ? "OK" : "FAILED"));
        
        // Report semantic errors and warnings
        for (const auto& err : analyzer.errors()) {
            std::cerr << "语义错误";
            if (err.location.line > 0) std::cerr << " (第 " << err.location.line << " 行)";
            std::cerr << ": " << err.message << "\n";
        }
        for (const auto& warn : analyzer.warnings()) {
            std::cerr << "警告";
            if (warn.location.line > 0) std::cerr << " (第 " << warn.location.line << " 行)";
            std::cerr << ": " << warn.message << "\n";
        }
        
        if (!semanticOK) {
            std::cerr << "错误: 语义分析失败\n";
            return 1;
        }
        
        if (doSemantic && !doIR && !doAsm && !doCompile) {
            std::cout << "=== Semantic Analysis ===\n";
            std::cout << "Status: Passed\n";
            std::cout << "Declarations: " << ast->declarations.size() << "\n";
            std::cout << "Errors: " << analyzer.errors().size() << "\n";
            std::cout << "Warnings: " << analyzer.warnings().size() << "\n";
            std::cout << "\n--- Symbol Summary ---\n";
            int funcCount = 0, varCount = 0, typeCount = 0;
            for (const auto& decl : ast->declarations) {
                if (dynamic_cast<ast::FunctionDecl*>(decl.get())) ++funcCount;
                else if (dynamic_cast<ast::VarDecl*>(decl.get())) ++varCount;
                else ++typeCount;
            }
            std::cout << "Functions: " << funcCount << "\n";
            std::cout << "Global Variables: " << varCount << "\n";
            std::cout << "Type Definitions: " << typeCount << "\n";
            return 0;
        }
        
        // 阶段 5：中间代码生成
        // - 遍历 AST 生成四元式
        // - 处理控制流、函数调用、表达式求值
        CDD_DBG_STAGE("IR Generation");
        semantic::IRGenerator irGen(analyzer.symbolTable());
        semantic::IRProgram irProgram = irGen.generate(ast.get());
        CDD_DBG("IR generation completed");
        
        // 输出 IR
        if (doIR && !doAsm && !doCompile) {
            std::cout << "\n=== Intermediate Representation (Quadruples) ===\n";
            for (const auto& func : irProgram.functions) {
                std::cout << "\nFunction: " << func.name << "\n";
                for (size_t i = 0; i < func.code.size(); ++i) {
                    std::cout << "  [" << i << "] " << func.code[i].toString() << "\n";
                }
            }
            return 0;
        }
        
        // 阶段 6：目标代码生成
        // - 将四元式翻译为 x86-64 汇编
        // - 处理寄存器分配、栈帧布局
        // - 遵循 System V AMD64 ABI
            CDD_DBG_STAGE("Code Generation");
            
            codegen::CodeGenerator codeGen(irProgram);
            std::string asmCode = codeGen.generate();
            CDD_DBG("Generated assembly size: " << asmCode.size() << " bytes");
            
            if (doCompile) {
                CDD_DBG_STAGE("Assembly & Linking");
                std::string asmFile = outputFile + ".s";
                CDD_DBG("Writing assembly to: " << asmFile);
                {
                    std::ofstream out(asmFile);
                    if (!out) {
                        std::cerr << "Error: Cannot write to " << asmFile << "\n";
                        return 1;
                    }
                    out << asmCode;
                }
                
                if (codegen::assembleAndLink(asmFile, outputFile)) {
                    std::cout << "Compiled successfully: " << outputFile << "\n";
                    return 0;
                } else {
                    std::cerr << "Error: Assembly/linking failed\n";
                    return 1;
                }
            }
        
        // 默认行为：输出汇编
        if (outputFile == "-" || outputFile == "a.out") {
            std::cout << asmCode;
        } else {
            std::ofstream out(outputFile);
            if (!out) {
                std::cerr << "Error: Cannot write to " << outputFile << "\n";
                return 1;
            }
            out << asmCode;
            std::cout << "Assembly written to " << outputFile << "\n";
        }

    } catch (const std::exception &e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
