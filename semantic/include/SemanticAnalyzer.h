/**
 * @file SemanticAnalyzer.h
 * @brief 语义分析器
 * @author mozihe
 * 
 * 遍历 AST 进行类型检查和语义验证。
 */

#ifndef CDD_SEMANTIC_ANALYZER_H
#define CDD_SEMANTIC_ANALYZER_H

#include "Ast.h"
#include "Type.h"
#include "SymbolTable.h"
#include <string>
#include <vector>

namespace cdd {
namespace semantic {

/**
 * @struct SemanticError
 * @brief 语义错误/警告信息
 */
struct SemanticError {
    SourceLocation location;  ///< 错误位置
    std::string message;      ///< 错误消息
    bool isWarning = false;   ///< 是否为警告
};

/**
 * @class SemanticAnalyzer
 * @brief 语义分析器
 * 
 * 主要功能：
 * 1. 类型检查
 * 2. 符号解析
 * 3. 作用域管理
 * 4. 语义约束验证
 */
class SemanticAnalyzer {
public:
    SemanticAnalyzer();
    ~SemanticAnalyzer();

    /**
     * @brief 分析翻译单元
     * @param unit AST 根节点
     * @return 无错误返回 true
     */
    bool analyze(ast::TranslationUnit* unit);

    /**
     * @brief 获取错误列表
     * @return 错误列表的常量引用
     */
    const std::vector<SemanticError>& errors() const { return errors_; }
    
    /**
     * @brief 获取警告列表
     * @return 警告列表的常量引用
     */
    const std::vector<SemanticError>& warnings() const { return warnings_; }
    
    /**
     * @brief 检查是否有错误
     * @return 有错误返回 true
     */
    bool hasErrors() const { return !errors_.empty(); }

    /**
     * @brief 获取符号表
     * @return 符号表引用
     */
    SymbolTable& symbolTable() { return symTable_; }

private:
    SymbolTable symTable_;
    std::vector<SemanticError> errors_;
    std::vector<SemanticError> warnings_;
    
    int loopDepth_ = 0;    ///< 当前循环嵌套深度
    int switchDepth_ = 0;  ///< 当前 switch 嵌套深度

    /**
     * @brief 报告错误
     * @param loc 错误位置
     * @param msg 错误消息
     */
    void error(const SourceLocation& loc, const std::string& msg);
    
    /**
     * @brief 报告警告
     * @param loc 警告位置
     * @param msg 警告消息
     */
    void warning(const SourceLocation& loc, const std::string& msg);

    // ==================== 声明分析 ====================
    
    /** @brief 分析声明 */
    void analyzeDecl(ast::Decl* decl);
    
    /** @brief 分析变量声明 */
    void analyzeVarDecl(ast::VarDecl* decl);
    
    /** @brief 分析函数声明/定义 */
    void analyzeFunctionDecl(ast::FunctionDecl* decl);
    
    /** @brief 分析结构体/联合体声明 */
    void analyzeRecordDecl(ast::RecordDecl* decl);
    
    /** @brief 分析枚举声明 */
    void analyzeEnumDecl(ast::EnumDecl* decl);
    
    /** @brief 分析 typedef 声明 */
    void analyzeTypedefDecl(ast::TypedefDecl* decl);
    
    // ==================== 语句分析 ====================
    
    /** @brief 分析语句 */
    void analyzeStmt(ast::Stmt* stmt);
    
    /** @brief 分析复合语句 */
    void analyzeCompoundStmt(ast::CompoundStmt* stmt);
    
    /** @brief 分析 if 语句 */
    void analyzeIfStmt(ast::IfStmt* stmt);
    
    /** @brief 分析 while 语句 */
    void analyzeWhileStmt(ast::WhileStmt* stmt);
    
    /** @brief 分析 do-while 语句 */
    void analyzeDoWhileStmt(ast::DoWhileStmt* stmt);
    
    /** @brief 分析 for 语句 */
    void analyzeForStmt(ast::ForStmt* stmt);
    
    /** @brief 分析 switch 语句 */
    void analyzeSwitchStmt(ast::SwitchStmt* stmt);
    
    /** @brief 分析 case 标签 */
    void analyzeCaseStmt(ast::CaseStmt* stmt);
    
    /** @brief 分析 default 标签 */
    void analyzeDefaultStmt(ast::DefaultStmt* stmt);
    
    /** @brief 分析 break 语句 */
    void analyzeBreakStmt(ast::BreakStmt* stmt);
    
    /** @brief 分析 continue 语句 */
    void analyzeContinueStmt(ast::ContinueStmt* stmt);
    
    /** @brief 分析 return 语句 */
    void analyzeReturnStmt(ast::ReturnStmt* stmt);

    // ==================== 表达式分析 ====================
    
    /**
     * @brief 分析表达式并返回其类型
     * @param expr 表达式 AST
     * @return 表达式类型
     */
    TypePtr analyzeExpr(ast::Expr* expr);
    
    TypePtr analyzeIntLiteral(ast::IntLiteral* expr);
    TypePtr analyzeFloatLiteral(ast::FloatLiteral* expr);
    TypePtr analyzeIdentExpr(ast::IdentExpr* expr);
    TypePtr analyzeBinaryExpr(ast::BinaryExpr* expr);
    TypePtr analyzeUnaryExpr(ast::UnaryExpr* expr);
    TypePtr analyzeCallExpr(ast::CallExpr* expr);
    TypePtr analyzeSubscriptExpr(ast::SubscriptExpr* expr);
    TypePtr analyzeMemberExpr(ast::MemberExpr* expr);
    TypePtr analyzeCastExpr(ast::CastExpr* expr);
    TypePtr analyzeConditionalExpr(ast::ConditionalExpr* expr);

    // ==================== 类型转换 ====================
    
    /**
     * @brief 将 AST 类型转换为语义类型
     * @param astType AST 类型节点
     * @return 语义类型
     */
    TypePtr resolveAstType(ast::Type* astType);
    
    /**
     * @brief 将语义类型转换为 AST 类型
     * @param semType 语义类型
     * @return AST 类型
     */
    ast::TypePtr convertToAstType(TypePtr semType);
    
    // ==================== 辅助方法 ====================
    
    /**
     * @brief 检查表达式是否可赋值（左值）
     * @param expr 表达式
     * @return 可赋值返回 true
     */
    bool checkAssignable(ast::Expr* expr);
    
    /**
     * @brief 检查类型是否可用作条件
     * @param type 类型
     * @param loc 位置（用于报错）
     * @return 有效返回 true
     */
    bool checkCondition(TypePtr type, const SourceLocation& loc);
    
    /**
     * @brief 执行整数提升
     * @param type 原类型
     * @return 提升后的类型
     */
    TypePtr performIntegralPromotions(TypePtr type);
    
    /**
     * @brief 求值整数常量表达式
     * @param expr 表达式 AST
     * @param[out] result 计算结果
     * @return 成功求值返回 true
     */
    bool evaluateConstantExpr(ast::Expr* expr, int64_t& result);
};

} // namespace semantic
} // namespace cdd

#endif // CDD_SEMANTIC_ANALYZER_H
