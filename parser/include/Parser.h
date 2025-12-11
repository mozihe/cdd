/**
 * @file Parser.h
 * @brief 递归下降语法分析器
 * @author mozihe
 * 
 * 将 Token 流解析为抽象语法树（AST）。
 * 实现 C 语言子集的完整语法，包括声明、语句和表达式。
 */

#ifndef CDD_PARSER_H
#define CDD_PARSER_H

#include "Lexer.h"
#include "Ast.h"
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <functional>

namespace cdd {

/**
 * @class ParseError
 * @brief 语法解析错误
 */
class ParseError : public std::runtime_error {
public:
    SourceLocation location;  ///< 错误位置

    ParseError(const std::string& msg, SourceLocation loc)
        : std::runtime_error(msg), location(loc) {}
};

/**
 * @class Parser
 * @brief 递归下降语法分析器
 * 
 * 主要功能：
 * 1. 解析声明（变量、函数、结构体、枚举、typedef）
 * 2. 解析语句（控制流、跳转、表达式语句）
 * 3. 解析表达式（按优先级递归下降）
 */
class Parser {
public:
    /**
     * @brief 构造解析器
     * @param lexer 词法分析器引用
     */
    explicit Parser(Lexer& lexer);

    /**
     * @brief 解析整个翻译单元
     * @return 翻译单元 AST 根节点
     */
    std::unique_ptr<ast::TranslationUnit> parseTranslationUnit();

    /**
     * @brief 获取解析错误列表
     * @return 错误列表的常量引用
     */
    const std::vector<ParseError>& errors() const { return errors_; }

    /**
     * @brief 检查是否有解析错误
     * @return 有错误返回 true
     */
    bool hasErrors() const { return !errors_.empty(); }

private:
    Lexer& lexer_;
    Token currentToken_;
    Token lookahead_;
    bool hasLookahead_ = false;

    std::vector<ParseError> errors_;
    std::set<std::string> typedefNames_;

    // ==================== Token 操作 ====================
    
    /** @brief 获取当前 Token 引用 */
    Token& current() { return currentToken_; }
    
    /** @brief 预览下一个 Token（不消费） */
    Token peek();
    
    /** @brief 消费当前 Token，前进到下一个 */
    void advance();
    
    /**
     * @brief 检查当前 Token 是否为指定类型
     * @param kind 期望的 Token 类型
     * @return 匹配返回 true
     */
    bool check(TokenKind kind);
    
    /**
     * @brief 若当前 Token 匹配则消费
     * @param kind 期望的 Token 类型
     * @return 匹配并消费返回 true
     */
    bool match(TokenKind kind);
    
    /**
     * @brief 期望指定类型的 Token，否则报错
     * @param kind 期望的 Token 类型
     * @param message 错误消息
     * @return 消费的 Token
     */
    Token expect(TokenKind kind, const std::string& message);
    
    /** @brief 是否到达文件末尾 */
    bool isAtEnd();

    // ==================== 错误处理 ====================
    
    /**
     * @brief 报告错误（使用当前位置）
     * @param message 错误消息
     */
    void error(const std::string& message);
    
    /**
     * @brief 在指定 Token 位置报告错误
     * @param token 错误 Token
     * @param message 错误消息
     */
    void errorAt(const Token& token, const std::string& message);
    
    /** @brief 错误恢复：跳过 Token 直到同步点 */
    void synchronize();

    // ==================== 类型判断 ====================
    
    /** @brief 当前 Token 是否为类型名（包括 typedef） */
    bool isTypeName();
    
    /** @brief 当前 Token 是否为声明说明符 */
    bool isDeclarationSpecifier();
    
    /** @brief 当前 Token 是否为类型说明符 */
    bool isTypeSpecifier();
    
    /** @brief 当前 Token 是否为类型限定符 */
    bool isTypeQualifier();
    
    /** @brief 当前 Token 是否为存储类说明符 */
    bool isStorageClassSpecifier();

    // ==================== 声明解析 ====================
    
    /**
     * @brief 解析声明
     * @return 声明 AST 列表（int a, b; 产生多个声明）
     */
    ast::DeclList parseDeclaration();

    /** @brief 声明说明符结构 */
    struct DeclSpec {
        ast::StorageClass storage = ast::StorageClass::None;
        ast::TypeQualifier qualifiers = ast::TypeQualifier::None;
        ast::TypePtr type;
        bool isTypedef = false;
    };
    
    /**
     * @brief 解析声明说明符
     * @return 声明说明符信息
     */
    DeclSpec parseDeclarationSpecifiers();

    /**
     * @brief 解析类型说明符
     * @param spec 声明说明符（用于累积类型信息）
     * @return 类型 AST
     */
    ast::TypePtr parseTypeSpecifier(DeclSpec& spec);

    /** @brief 解析结构体或联合体说明符 */
    ast::TypePtr parseStructOrUnionSpecifier();

    /** @brief 解析结构体成员列表 */
    std::vector<std::unique_ptr<ast::FieldDecl>> parseStructDeclarationList();

    /** @brief 解析枚举说明符 */
    ast::TypePtr parseEnumSpecifier();

    /** @brief 声明符信息结构 */
    struct Declarator {
        std::string name;                                       ///< 声明的名称
        ast::TypePtr type;                                      ///< 完整类型
        std::vector<std::unique_ptr<ast::ParamDecl>> params;    ///< 函数参数
        bool isFunction = false;                                ///< 是否为函数
        bool isVariadic = false;                                ///< 是否为变参
        SourceLocation location;                                ///< 位置
    };
    
    /**
     * @brief 解析声明符
     * @param baseType 基础类型
     * @return 声明符信息
     */
    Declarator parseDeclarator(ast::TypePtr baseType);

    /**
     * @brief 解析指针修饰
     * @param baseType 被指向的类型
     * @return 指针类型
     */
    ast::TypePtr parsePointer(ast::TypePtr baseType);

    /**
     * @brief 解析直接声明符
     * @param baseType 基础类型
     * @return 声明符信息
     */
    Declarator parseDirectDeclarator(ast::TypePtr baseType);

    /**
     * @brief 解析抽象声明符
     * @param baseType 基础类型
     * @return 完整类型
     */
    ast::TypePtr parseAbstractDeclarator(ast::TypePtr baseType);

    /**
     * @brief 解析参数列表
     * @param isVariadic 输出：是否为变参函数
     * @return 参数声明列表
     */
    std::vector<std::unique_ptr<ast::ParamDecl>> parseParameterTypeList(bool& isVariadic);

    /** @brief 解析单个参数声明 */
    std::unique_ptr<ast::ParamDecl> parseParameterDeclaration();

    /** @brief 解析类型名（用于 sizeof、强制转换） */
    ast::TypePtr parseTypeName();

    /** @brief 解析初始化器 */
    ast::ExprPtr parseInitializer();

    /** @brief 解析初始化器列表 */
    ast::ExprPtr parseInitializerList();

    // ==================== 语句解析 ====================
    
    /** @brief 解析语句 */
    ast::StmtPtr parseStatement();

    /** @brief 解析标签语句（label:, case, default） */
    ast::StmtPtr parseLabeledStatement();

    /** @brief 解析复合语句 { ... } */
    std::unique_ptr<ast::CompoundStmt> parseCompoundStatement();

    /** @brief 解析选择语句（if, switch） */
    ast::StmtPtr parseSelectionStatement();

    /** @brief 解析循环语句（while, do, for） */
    ast::StmtPtr parseIterationStatement();

    /** @brief 解析跳转语句（goto, continue, break, return） */
    ast::StmtPtr parseJumpStatement();

    /** @brief 解析表达式语句 */
    ast::StmtPtr parseExpressionStatement();

    // ==================== 表达式解析（按优先级） ====================
    
    /** @brief 解析逗号表达式 */
    ast::ExprPtr parseExpression();

    /** @brief 解析赋值表达式 */
    ast::ExprPtr parseAssignmentExpression();

    /** @brief 解析条件表达式（三元运算符） */
    ast::ExprPtr parseConditionalExpression();

    /** @brief 解析逻辑或表达式 */
    ast::ExprPtr parseLogicalOrExpression();

    /** @brief 解析逻辑与表达式 */
    ast::ExprPtr parseLogicalAndExpression();

    /** @brief 解析按位或表达式 */
    ast::ExprPtr parseInclusiveOrExpression();

    /** @brief 解析按位异或表达式 */
    ast::ExprPtr parseExclusiveOrExpression();

    /** @brief 解析按位与表达式 */
    ast::ExprPtr parseAndExpression();

    /** @brief 解析相等性表达式 */
    ast::ExprPtr parseEqualityExpression();

    /** @brief 解析关系表达式 */
    ast::ExprPtr parseRelationalExpression();

    /** @brief 解析移位表达式 */
    ast::ExprPtr parseShiftExpression();

    /** @brief 解析加法表达式 */
    ast::ExprPtr parseAdditiveExpression();

    /** @brief 解析乘法表达式 */
    ast::ExprPtr parseMultiplicativeExpression();

    /** @brief 解析强制类型转换表达式 */
    ast::ExprPtr parseCastExpression();

    /** @brief 解析一元表达式 */
    ast::ExprPtr parseUnaryExpression();

    /** @brief 解析后缀表达式 */
    ast::ExprPtr parsePostfixExpression();

    /** @brief 解析基本表达式 */
    ast::ExprPtr parsePrimaryExpression();

    /** @brief 解析函数调用参数列表 */
    ast::ExprList parseArgumentExpressionList();

    // ==================== 运算符辅助 ====================
    
    /**
     * @brief TokenKind 转换为 BinaryOp
     * @param kind Token 类型
     * @return 对应的二元运算符
     */
    ast::BinaryOp tokenToBinaryOp(TokenKind kind);
    
    /**
     * @brief TokenKind 转换为 UnaryOp
     * @param kind Token 类型
     * @return 对应的一元运算符
     */
    ast::UnaryOp tokenToUnaryOp(TokenKind kind);
    
    /**
     * @brief 判断是否为赋值运算符
     * @param kind Token 类型
     * @return 是赋值运算符返回 true
     */
    bool isAssignmentOperator(TokenKind kind);
    
    /**
     * @brief 复合赋值运算符转换为基础运算符
     * @param kind 复合赋值运算符（如 +=）
     * @return 基础运算符（如 Add）
     */
    ast::BinaryOp assignOpToBinaryOp(TokenKind kind);
};

} // namespace cdd

#endif // CDD_PARSER_H
