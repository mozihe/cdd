/**
 * @file IRGenerator.h
 * @brief 中间代码生成器
 * @author mozihe
 * 
 * 将 AST 转换为四元式三地址码中间表示。
 */

#ifndef CDD_SEMANTIC_IR_GENERATOR_H
#define CDD_SEMANTIC_IR_GENERATOR_H

#include "Ast.h"
#include "Type.h"
#include "SymbolTable.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace cdd {
namespace semantic {

/**
 * @enum IROpcode
 * @brief 四元式操作码
 */
enum class IROpcode {
    // 整数算术运算
    Add, Sub, Mul, Div, Mod, Neg,
    // 浮点算术运算
    FAdd, FSub, FMul, FDiv, FNeg,
    // 位运算
    BitAnd, BitOr, BitXor, BitNot, Shl, Shr,
    // 整数比较运算
    Eq, Ne, Lt, Le, Gt, Ge,
    // 浮点比较运算
    FEq, FNe, FLt, FLe, FGt, FGe,
    // 逻辑运算
    LogicalAnd, LogicalOr, LogicalNot,
    // 数据移动
    Assign, Load, Store, LoadAddr,
    // 数组和结构体
    IndexAddr, MemberAddr,
    // 控制流
    Label, Jump, JumpTrue, JumpFalse,
    // 函数调用
    Param, Call, Return,
    // 类型转换
    IntToFloat, FloatToInt,
    IntExtend, IntTrunc, PtrToInt, IntToPtr,
    // Switch 支持
    Switch, Case,
    // 其他
    Nop, Comment,
};

/**
 * @enum OperandKind
 * @brief 操作数类型
 */
enum class OperandKind {
    None,        ///< 空操作数
    Temp,        ///< 临时变量
    Variable,    ///< 用户变量
    IntConst,    ///< 整数常量
    FloatConst,  ///< 浮点常量
    StringConst, ///< 字符串常量
    Label,       ///< 标签
    Global,      ///< 全局变量
};

/**
 * @struct Operand
 * @brief 四元式操作数
 */
struct Operand {
    OperandKind kind = OperandKind::None;
    std::string name;
    int64_t intValue = 0;
    double floatValue = 0.0;
    TypePtr type;
    
    Operand() = default;
    
    /** @brief 创建空操作数 */
    static Operand none() { return Operand(); }
    
    /** @brief 创建临时变量操作数 */
    static Operand temp(const std::string& name, TypePtr type);
    
    /** @brief 创建用户变量操作数 */
    static Operand variable(const std::string& name, TypePtr type);
    
    /** @brief 创建整数常量操作数 */
    static Operand intConst(int64_t value, TypePtr type = nullptr);
    
    /** @brief 创建浮点常量操作数 */
    static Operand floatConst(double value, TypePtr type = nullptr);
    
    /** @brief 创建字符串常量操作数 */
    static Operand stringConst(const std::string& value);
    
    /** @brief 创建标签操作数 */
    static Operand label(const std::string& name);
    
    /** @brief 创建全局变量操作数 */
    static Operand global(const std::string& name, TypePtr type);
    
    bool isNone() const { return kind == OperandKind::None; }
    bool isTemp() const { return kind == OperandKind::Temp; }
    bool isConst() const;
    
    /** @brief 转换为字符串表示 */
    std::string toString() const;
};

/**
 * @struct Quadruple
 * @brief 四元式结构
 * 
 * 格式：result = arg1 op arg2
 */
struct Quadruple {
    IROpcode opcode;
    Operand result;
    Operand arg1;
    Operand arg2;
    
    Quadruple(IROpcode op, Operand res = Operand::none(),
              Operand a1 = Operand::none(), Operand a2 = Operand::none())
        : opcode(op), result(res), arg1(a1), arg2(a2) {}
    
    std::string toString() const;
};

/**
 * @struct FunctionIR
 * @brief 函数的中间表示
 */
struct FunctionIR {
    std::string name;                                    ///< 函数名
    TypePtr returnType;                                  ///< 返回类型
    std::vector<std::pair<std::string, TypePtr>> parameters; ///< 参数列表
    std::vector<Quadruple> code;                         ///< 四元式序列
    int stackSize = 0;                                   ///< 栈帧大小
    bool isVariadic = false;                             ///< 是否为变参函数
};

/**
 * @struct GlobalInitValue
 * @brief 全局变量初始化值
 * 
 * 用于存储编译期常量初始化值。
 */
struct GlobalInitValue {
    enum class Kind {
        Integer,    ///< 整数常量
        Float,      ///< 浮点常量
        String,     ///< 字符串标签引用
        Address,    ///< 地址（全局变量/函数地址）
        Zero        ///< 零初始化
    };
    
    Kind kind = Kind::Zero;
    int64_t intValue = 0;
    double floatValue = 0.0;
    std::string strLabel;
    int size = 8;           ///< 值大小（字节）
    
    static GlobalInitValue integer(int64_t v, int sz = 8);
    static GlobalInitValue floating(double v, int sz = 8);
    static GlobalInitValue string(const std::string& label);
    static GlobalInitValue address(const std::string& name);
    static GlobalInitValue zero(int sz);
};

/**
 * @struct GlobalVar
 * @brief 全局变量
 */
struct GlobalVar {
    std::string name;
    TypePtr type;
    bool hasInitializer = false;
    bool isExtern = false;                      ///< 是否为 extern 声明（不生成定义）
    std::vector<GlobalInitValue> initValues;    ///< 初始化值列表（支持数组/结构体）
};

/**
 * @struct IRProgram
 * @brief 完整的 IR 程序
 */
struct IRProgram {
    std::vector<GlobalVar> globals;                      ///< 全局变量
    std::vector<FunctionIR> functions;                   ///< 函数列表
    std::vector<std::pair<std::string, std::string>> stringLiterals; ///< 字符串字面量
};

/**
 * @class IRGenerator
 * @brief 中间代码生成器
 * 
 * 遍历 AST 生成四元式中间代码。
 */
class IRGenerator {
public:
    /**
     * @brief 构造 IR 生成器
     * @param symTable 符号表引用
     */
    IRGenerator(SymbolTable& symTable);
    ~IRGenerator();

    /**
     * @brief 生成整个程序的 IR
     * @param unit 翻译单元 AST
     * @return IR 程序
     */
    IRProgram generate(ast::TranslationUnit* unit);

private:
    SymbolTable& symTable_;
    IRProgram program_;
    FunctionIR* currentFunc_ = nullptr;
    
    int tempCounter_ = 0;
    int labelCounter_ = 0;
    int stringCounter_ = 0;
    int varCounter_ = 0;
    
    std::unordered_map<Symbol*, std::string> varIRNames_;
    std::unordered_map<std::string, int64_t> enumConstValues_;
    
    std::vector<std::string> breakTargets_;
    std::vector<std::string> continueTargets_;
    
    struct SwitchInfo {
        Operand condition;
        std::string defaultLabel;
        std::string endLabel;
        std::vector<std::pair<int64_t, std::string>> cases;
    };
    std::vector<SwitchInfo> switchStack_;

    /** @brief 生成新的临时变量名 */
    std::string newTemp();
    
    /** @brief 生成新的标签名 */
    std::string newLabel(const std::string& prefix = "L");
    
    /** @brief 添加字符串字面量，返回其标签 */
    std::string addStringLiteral(const std::string& value);
    
    /** @brief 发射四元式 */
    void emit(IROpcode op, Operand result = Operand::none(),
              Operand arg1 = Operand::none(), Operand arg2 = Operand::none());
    
    /** @brief 发射标签 */
    void emitLabel(const std::string& label);
    
    /** @brief 发射无条件跳转 */
    void emitJump(const std::string& label);
    
    /** @brief 发射条件为真跳转 */
    void emitJumpTrue(const Operand& cond, const std::string& label);
    
    /** @brief 发射条件为假跳转 */
    void emitJumpFalse(const Operand& cond, const std::string& label);
    
    /** @brief 发射注释 */
    void emitComment(const std::string& comment);

    /** @brief 生成声明的 IR */
    void genDecl(ast::Decl* decl);
    
    /** @brief 生成局部变量声明 */
    void genVarDecl(ast::VarDecl* decl);
    
    /** @brief 生成函数定义 */
    void genFunctionDecl(ast::FunctionDecl* decl);
    
    /** @brief 生成全局变量 */
    void genGlobalVar(ast::VarDecl* decl);
    
    /** @brief 生成语句的 IR */
    void genStmt(ast::Stmt* stmt);
    
    /** @brief 生成复合语句 */
    void genCompoundStmt(ast::CompoundStmt* stmt);
    
    /** @brief 生成 if 语句 */
    void genIfStmt(ast::IfStmt* stmt);
    
    /** @brief 生成 while 循环 */
    void genWhileStmt(ast::WhileStmt* stmt);
    
    /** @brief 生成 do-while 循环 */
    void genDoWhileStmt(ast::DoWhileStmt* stmt);
    
    /** @brief 生成 for 循环 */
    void genForStmt(ast::ForStmt* stmt);
    
    /** @brief 生成 switch 语句 */
    void genSwitchStmt(ast::SwitchStmt* stmt);
    
    /** @brief 生成 case 标签 */
    void genCaseStmt(ast::CaseStmt* stmt);
    
    /** @brief 生成 default 标签 */
    void genDefaultStmt(ast::DefaultStmt* stmt);
    
    /** @brief 生成 break 语句 */
    void genBreakStmt(ast::BreakStmt* stmt);
    
    /** @brief 生成 continue 语句 */
    void genContinueStmt(ast::ContinueStmt* stmt);
    
    /** @brief 生成 return 语句 */
    void genReturnStmt(ast::ReturnStmt* stmt);

    /**
     * @brief 生成表达式的 IR
     * @param expr 表达式 AST
     * @return 表达式结果操作数
     */
    Operand genExpr(ast::Expr* expr);
    
    Operand genIntLiteral(ast::IntLiteral* expr);
    Operand genFloatLiteral(ast::FloatLiteral* expr);
    Operand genCharLiteral(ast::CharLiteral* expr);
    Operand genStringLiteral(ast::StringLiteral* expr);
    Operand genIdentExpr(ast::IdentExpr* expr);
    Operand genBinaryExpr(ast::BinaryExpr* expr);
    Operand genUnaryExpr(ast::UnaryExpr* expr);
    Operand genCallExpr(ast::CallExpr* expr);
    Operand genSubscriptExpr(ast::SubscriptExpr* expr);
    Operand genMemberExpr(ast::MemberExpr* expr);
    Operand genCastExpr(ast::CastExpr* expr);
    Operand genSizeofExpr(ast::UnaryExpr* expr);
    Operand genConditionalExpr(ast::ConditionalExpr* expr);
    
    /**
     * @brief 生成左值地址
     * @param expr 左值表达式
     * @return 地址操作数
     */
    Operand genLValueAddr(ast::Expr* expr);
    
    /** @brief 生成赋值表达式 */
    Operand genAssignment(ast::BinaryExpr* expr);
    
    /** @brief 生成逻辑与（短路求值） */
    Operand genLogicalAnd(ast::BinaryExpr* expr);
    
    /** @brief 生成逻辑或（短路求值） */
    Operand genLogicalOr(ast::BinaryExpr* expr);
    
    /** @brief AST 类型转换为语义类型 */
    TypePtr astTypeToSemType(ast::Type* astType);
    
    /** @brief 获取表达式的语义类型 */
    TypePtr getExprSemType(ast::Expr* expr);
    
    /** @brief 生成类型转换代码 */
    Operand convertType(Operand src, TypePtr targetType);
    
    /**
     * @brief 收集全局变量初始化值
     * @param init 初始化表达式
     * @param type 目标类型
     * @param values 输出初始化值列表
     */
    void collectGlobalInitializer(ast::Expr* init, TypePtr type, 
                                  std::vector<GlobalInitValue>& values);
    
    /**
     * @brief 计算常量表达式的值
     * @param expr 表达式 AST
     * @param result 输出计算结果
     * @return 成功求值返回 true
     */
    bool evaluateConstExpr(ast::Expr* expr, int64_t& result);
};

/**
 * @brief 获取操作码的字符串名称
 * @param op 操作码
 * @return 操作码名称
 */
const char* opcodeToString(IROpcode op);

} // namespace semantic
} // namespace cdd

#endif // CDD_SEMANTIC_IR_GENERATOR_H
