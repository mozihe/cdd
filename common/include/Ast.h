/**
 * @file Ast.h
 * @brief 抽象语法树节点定义
 * @author mozihe
 * 
 * 定义 CDD 编译器的 AST 节点类型，包括：
 * - 类型节点（BasicType、PointerType、ArrayType 等）
 * - 表达式节点（字面量、一元/二元表达式、函数调用等）
 * - 语句节点（控制流、跳转、复合语句等）
 * - 声明节点（变量、函数、结构体、枚举等）
 */

#ifndef CDD_AST_NEW_H
#define CDD_AST_NEW_H

#include "TokenKind.h"
#include "SourceLocation.h"
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <iostream>

namespace cdd {
namespace ast {

// 前向声明
struct Expr;
struct Stmt;
struct Decl;
struct Type;
struct FieldDecl;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;
using TypePtr = std::unique_ptr<Type>;

using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;
using DeclList = std::vector<DeclPtr>;

//=============================================================================
// 基础节点
//=============================================================================

/**
 * @brief AST 节点基类
 */
struct Node {
    SourceLocation location;

    Node() = default;
    explicit Node(SourceLocation loc) : location(loc) {}
    virtual ~Node() = default;
};

//=============================================================================
// 类型节点
//=============================================================================

/**
 * @brief 类型修饰符
 */
enum class TypeQualifier {
    None = 0,
    Const = 1,
    Volatile = 2,
    Restrict = 4,
};

inline TypeQualifier operator|(TypeQualifier a, TypeQualifier b) {
    return static_cast<TypeQualifier>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool hasQualifier(TypeQualifier q, TypeQualifier test) {
    return (static_cast<int>(q) & static_cast<int>(test)) != 0;
}

/**
 * @brief 存储类说明符
 */
enum class StorageClass {
    None,
    Typedef,
    Extern,
    Static,
    Auto,
    Register,
};

/**
 * @brief 基本类型种类
 */
enum class BasicTypeKind {
    Void,
    Char,
    Short,
    Int,
    Long,
    LongLong,
    Float,
    Double,
    LongDouble,
    // 无符号变体
    UChar,
    UShort,
    UInt,
    ULong,
    ULongLong,
    // 有符号变体 (显式)
    SChar,
};

/**
 * @brief 类型节点基类
 */
struct Type : Node {
    TypeQualifier qualifiers = TypeQualifier::None;

    using Node::Node;

    virtual TypePtr clone() const = 0;
};

/**
 * @brief 基本类型
 */
struct BasicType : Type {
    BasicTypeKind kind { BasicTypeKind::Int };

    BasicType(BasicTypeKind k) : kind(k) {}
    BasicType(SourceLocation loc, BasicTypeKind k) : Type(loc), kind(k) {}

    TypePtr clone() const override {
        auto copy = std::make_unique<BasicType>(location, kind);
        copy->qualifiers = qualifiers;
        return copy;
    }
};

/**
 * @brief 指针类型
 */
struct PointerType : Type {
    TypePtr pointee;        ///< 指向的类型

    PointerType(TypePtr base) : pointee(std::move(base)) {}

    TypePtr clone() const override {
        auto copy = std::make_unique<PointerType>(pointee ? pointee->clone() : nullptr);
        copy->qualifiers = qualifiers;
        return copy;
    }
};

/**
 * @brief 数组类型
 */
struct ArrayType : Type {
    TypePtr elementType;    ///< 元素类型
    ExprPtr size;           ///< 数组大小（可选，nullptr 表示不完整数组）

    ArrayType(TypePtr elem, ExprPtr sz = nullptr)
        : elementType(std::move(elem)), size(std::move(sz)) {}

    TypePtr clone() const override {
        // 注意：数组大小表达式不克隆（需要时可扩展）
        auto copy = std::make_unique<ArrayType>(elementType ? elementType->clone() : nullptr, nullptr);
        copy->qualifiers = qualifiers;
        return copy;
    }
};

/**
 * @brief 函数类型
 */
struct FunctionType : Type {
    TypePtr returnType;
    std::vector<TypePtr> paramTypes;
    bool isVariadic = false;

    FunctionType(TypePtr ret) : returnType(std::move(ret)) {}

    TypePtr clone() const override {
        auto copy = std::make_unique<FunctionType>(returnType ? returnType->clone() : nullptr);
        copy->isVariadic = isVariadic;
        copy->qualifiers = qualifiers;
        for (const auto& pt : paramTypes) {
            copy->paramTypes.push_back(pt ? pt->clone() : nullptr);
        }
        return copy;
    }
};

/**
 * @brief 结构体/联合体类型
 */
struct RecordType : Type {
    bool isUnion;
    std::string name;       ///< 可能为空（匿名）
    std::vector<std::unique_ptr<FieldDecl>> fields;  ///< 结构体定义的字段（可为空）

    RecordType(bool isUnion, std::string n = "")
        : isUnion(isUnion), name(std::move(n)) {}

    // clone 在 FieldDecl 定义后实现
    TypePtr clone() const override;
};

/**
 * @brief 枚举类型
 */
struct EnumConstantDecl;  // 前向声明

struct EnumType : Type {
    std::string name;       ///< 可能为空（匿名）
    std::vector<std::unique_ptr<EnumConstantDecl>> constants;  ///< 枚举常量（可为空）

    EnumType(std::string n = "") : name(std::move(n)) {}

    // clone 在 EnumConstantDecl 定义后实现
    TypePtr clone() const override;
};

/**
 * @brief typedef 名称类型
 */
struct TypedefType : Type {
    std::string name;

    TypedefType(std::string n) : name(std::move(n)) {}

    TypePtr clone() const override {
        auto copy = std::make_unique<TypedefType>(name);
        copy->qualifiers = qualifiers;
        return copy;
    }
};

//=============================================================================
// 表达式节点
//=============================================================================

/**
 * @brief 表达式基类
 */
struct Expr : Node {
    TypePtr exprType;       ///< 表达式的类型（语义分析后填充）
    bool isLValue = false;  ///< 是否为左值

    using Node::Node;
};

/**
 * @brief 整数字面量
 */
struct IntLiteral : Expr {
    int64_t value;
    bool isUnsigned = false;
    bool isLong = false;
    bool isLongLong = false;

    IntLiteral(SourceLocation loc, int64_t v)
        : Expr(loc), value(v) {}
};

/**
 * @brief 浮点字面量
 */
struct FloatLiteral : Expr {
    double value;
    bool isFloat = false;   ///< true = float, false = double

    FloatLiteral(SourceLocation loc, double v)
        : Expr(loc), value(v) {}
};

/**
 * @brief 字符字面量
 */
struct CharLiteral : Expr {
    char value;

    CharLiteral(SourceLocation loc, char v)
        : Expr(loc), value(v) {}
};

/**
 * @brief 字符串字面量
 */
struct StringLiteral : Expr {
    std::string value;

    StringLiteral(SourceLocation loc, std::string v)
        : Expr(loc), value(std::move(v)) {}
};

/**
 * @brief 标识符表达式
 */
struct IdentExpr : Expr {
    std::string name;

    IdentExpr(SourceLocation loc, std::string n)
        : Expr(loc), name(std::move(n)) {}
};

/**
 * @brief 一元运算符
 */
enum class UnaryOp {
    Plus,       // +
    Minus,      // -
    Not,        // !
    BitNot,     // ~
    Deref,      // *
    AddrOf,     // &
    PreInc,     // ++x
    PreDec,     // --x
    PostInc,    // x++
    PostDec,    // x--
    Sizeof,     // sizeof
};

/**
 * @brief 一元表达式
 */
struct UnaryExpr : Expr {
    UnaryOp op;
    ExprPtr operand;

    UnaryExpr(SourceLocation loc, UnaryOp o, ExprPtr opnd)
        : Expr(loc), op(o), operand(std::move(opnd)) {}
};

/**
 * @brief sizeof(type) 表达式
 */
struct SizeofTypeExpr : Expr {
    TypePtr sizedType;

    SizeofTypeExpr(SourceLocation loc, TypePtr t)
        : Expr(loc), sizedType(std::move(t)) {}
};

/**
 * @brief 二元运算符
 */
enum class BinaryOp {
    // 算术
    Add, Sub, Mul, Div, Mod,
    // 位运算
    BitAnd, BitOr, BitXor, Shl, Shr,
    // 关系
    Lt, Gt, Le, Ge, Eq, Ne,
    // 逻辑
    LogAnd, LogOr,
    // 赋值
    Assign,
    AddAssign, SubAssign, MulAssign, DivAssign, ModAssign,
    AndAssign, OrAssign, XorAssign, ShlAssign, ShrAssign,
    // 逗号
    Comma,
};

/**
 * @brief 二元表达式
 */
struct BinaryExpr : Expr {
    BinaryOp op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(SourceLocation loc, BinaryOp o, ExprPtr l, ExprPtr r)
        : Expr(loc), op(o), left(std::move(l)), right(std::move(r)) {}
};

/**
 * @brief 条件表达式 (a ? b : c)
 */
struct ConditionalExpr : Expr {
    ExprPtr condition;
    ExprPtr thenExpr;
    ExprPtr elseExpr;

    ConditionalExpr(SourceLocation loc, ExprPtr cond, ExprPtr t, ExprPtr e)
        : Expr(loc), condition(std::move(cond)),
          thenExpr(std::move(t)), elseExpr(std::move(e)) {}
};

/**
 * @brief 类型转换表达式 (type)expr
 */
struct CastExpr : Expr {
    TypePtr targetType;
    ExprPtr operand;

    CastExpr(SourceLocation loc, TypePtr t, ExprPtr e)
        : Expr(loc), targetType(std::move(t)), operand(std::move(e)) {}
};

/**
 * @brief 数组下标表达式 a[i]
 */
struct SubscriptExpr : Expr {
    ExprPtr array;
    ExprPtr index;

    SubscriptExpr(SourceLocation loc, ExprPtr arr, ExprPtr idx)
        : Expr(loc), array(std::move(arr)), index(std::move(idx)) {}
};

/**
 * @brief 函数调用表达式 f(args)
 */
struct CallExpr : Expr {
    ExprPtr callee;
    ExprList arguments;

    CallExpr(SourceLocation loc, ExprPtr fn, ExprList args)
        : Expr(loc), callee(std::move(fn)), arguments(std::move(args)) {}
};

/**
 * @brief 成员访问表达式 a.member
 */
struct MemberExpr : Expr {
    ExprPtr object;
    std::string member;
    bool isArrow;           ///< true = ->, false = .

    MemberExpr(SourceLocation loc, ExprPtr obj, std::string mem, bool arrow)
        : Expr(loc), object(std::move(obj)), member(std::move(mem)), isArrow(arrow) {}
};

/**
 * @brief 初始化列表 {1, 2, 3}
 */
struct InitListExpr : Expr {
    ExprList elements;

    InitListExpr(SourceLocation loc, ExprList elems)
        : Expr(loc), elements(std::move(elems)) {}
};

/**
 * @brief 指定初始化器 .field = value 或 [index] = value
 */
struct DesignatedInitExpr : Expr {
    struct Designator {
        bool isField;           ///< true = .field, false = [index]
        std::string field;
        ExprPtr index;
    };
    std::vector<Designator> designators;
    ExprPtr init;

    DesignatedInitExpr(SourceLocation loc, std::vector<Designator> desigs, ExprPtr val)
        : Expr(loc), designators(std::move(desigs)), init(std::move(val)) {}
};

//=============================================================================
// 语句节点
//=============================================================================

/**
 * @brief 语句基类
 */
struct Stmt : Node {
    using Node::Node;
};

/**
 * @brief 表达式语句
 */
struct ExprStmt : Stmt {
    ExprPtr expr;           ///< 可能为 nullptr（空语句 ;）

    ExprStmt(SourceLocation loc, ExprPtr e = nullptr)
        : Stmt(loc), expr(std::move(e)) {}
};

/**
 * @brief 复合语句 { ... }
 */
struct CompoundStmt : Stmt {
    std::vector<std::variant<StmtPtr, DeclPtr>> items;

    CompoundStmt(SourceLocation loc) : Stmt(loc) {}
};

/**
 * @brief if 语句
 */
struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr thenStmt;
    StmtPtr elseStmt;       ///< 可能为 nullptr

    IfStmt(SourceLocation loc, ExprPtr cond, StmtPtr t, StmtPtr e = nullptr)
        : Stmt(loc), condition(std::move(cond)),
          thenStmt(std::move(t)), elseStmt(std::move(e)) {}
};

/**
 * @brief switch 语句
 */
struct SwitchStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;

    SwitchStmt(SourceLocation loc, ExprPtr cond, StmtPtr b)
        : Stmt(loc), condition(std::move(cond)), body(std::move(b)) {}
};

/**
 * @brief case 标签
 */
struct CaseStmt : Stmt {
    ExprPtr value;          ///< 常量表达式
    StmtPtr stmt;
    mutable std::string label;  ///< IR 生成时使用的标签

    CaseStmt(SourceLocation loc, ExprPtr val, StmtPtr s)
        : Stmt(loc), value(std::move(val)), stmt(std::move(s)) {}
};

/**
 * @brief default 标签
 */
struct DefaultStmt : Stmt {
    StmtPtr stmt;
    mutable std::string label;  ///< IR 生成时使用的标签

    DefaultStmt(SourceLocation loc, StmtPtr s)
        : Stmt(loc), stmt(std::move(s)) {}
};

/**
 * @brief while 语句
 */
struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;

    WhileStmt(SourceLocation loc, ExprPtr cond, StmtPtr b)
        : Stmt(loc), condition(std::move(cond)), body(std::move(b)) {}
};

/**
 * @brief do-while 语句
 */
struct DoWhileStmt : Stmt {
    StmtPtr body;
    ExprPtr condition;

    DoWhileStmt(SourceLocation loc, StmtPtr b, ExprPtr cond)
        : Stmt(loc), body(std::move(b)), condition(std::move(cond)) {}
};

/**
 * @brief for 语句
 */
struct ForStmt : Stmt {
    std::variant<StmtPtr, DeclList> init;    ///< 初始化（表达式语句或声明列表）
    ExprPtr condition;      ///< 可能为 nullptr
    ExprPtr increment;      ///< 可能为 nullptr
    StmtPtr body;

    ForStmt(SourceLocation loc) : Stmt(loc) {}
};

/**
 * @brief goto 语句
 */
struct GotoStmt : Stmt {
    std::string label;

    GotoStmt(SourceLocation loc, std::string lbl)
        : Stmt(loc), label(std::move(lbl)) {}
};

/**
 * @brief continue 语句
 */
struct ContinueStmt : Stmt {
    ContinueStmt(SourceLocation loc) : Stmt(loc) {}
};

/**
 * @brief break 语句
 */
struct BreakStmt : Stmt {
    BreakStmt(SourceLocation loc) : Stmt(loc) {}
};

/**
 * @brief return 语句
 */
struct ReturnStmt : Stmt {
    ExprPtr value;          ///< 可能为 nullptr

    ReturnStmt(SourceLocation loc, ExprPtr v = nullptr)
        : Stmt(loc), value(std::move(v)) {}
};

/**
 * @brief 标签语句
 */
struct LabelStmt : Stmt {
    std::string label;
    StmtPtr stmt;

    LabelStmt(SourceLocation loc, std::string lbl, StmtPtr s)
        : Stmt(loc), label(std::move(lbl)), stmt(std::move(s)) {}
};

//=============================================================================
// 声明节点
//=============================================================================

/**
 * @brief 声明基类
 */
struct Decl : Node {
    StorageClass storage = StorageClass::None;
    std::string name;

    using Node::Node;
};

/**
 * @brief 变量声明
 */
struct VarDecl : Decl {
    TypePtr type;
    ExprPtr initializer;    ///< 可能为 nullptr

    VarDecl(SourceLocation loc, std::string n, TypePtr t, ExprPtr init = nullptr)
        : Decl() {
        this->location = loc;
        this->name = std::move(n);
        type = std::move(t);
        initializer = std::move(init);
    }
};

/**
 * @brief 函数参数声明
 */
struct ParamDecl : Decl {
    TypePtr type;

    ParamDecl(SourceLocation loc, std::string n, TypePtr t)
        : Decl() {
        this->location = loc;
        this->name = std::move(n);
        type = std::move(t);
    }
};

/**
 * @brief 函数声明/定义
 */
struct FunctionDecl : Decl {
    TypePtr returnType;
    std::vector<std::unique_ptr<ParamDecl>> params;
    bool isVariadic = false;
    std::unique_ptr<CompoundStmt> body;     ///< nullptr 表示只是声明

    FunctionDecl(SourceLocation loc, std::string n, TypePtr ret)
        : Decl() {
        this->location = loc;
        this->name = std::move(n);
        returnType = std::move(ret);
    }

    bool isDefinition() const { return body != nullptr; }
};

/**
 * @brief 结构体/联合体成员
 */
struct FieldDecl : Decl {
    TypePtr type;
    ExprPtr bitWidth;       ///< 位域宽度，可能为 nullptr

    FieldDecl(SourceLocation loc, std::string n, TypePtr t, ExprPtr bits = nullptr)
        : Decl() {
        this->location = loc;
        this->name = std::move(n);
        type = std::move(t);
        bitWidth = std::move(bits);
    }
};

// RecordType::clone() 的实现（需要在 FieldDecl 之后）
inline TypePtr RecordType::clone() const {
    auto copy = std::make_unique<RecordType>(isUnion, name);
    copy->qualifiers = qualifiers;
    // 克隆 fields（对于匿名结构体/联合体，字段信息必须保留）
    for (const auto& field : fields) {
        if (field) {
            auto fieldCopy = std::make_unique<FieldDecl>(
                field->location, 
                field->name, 
                field->type ? field->type->clone() : nullptr,
                nullptr  // bitWidth 通常不需要克隆
            );
            copy->fields.push_back(std::move(fieldCopy));
        }
    }
    return copy;
}

/**
 * @brief 结构体/联合体声明
 */
struct RecordDecl : Decl {
    bool isUnion;
    std::vector<std::unique_ptr<FieldDecl>> fields;

    RecordDecl(SourceLocation loc, bool isUnion, std::string n = "")
        : Decl(), isUnion(isUnion) {
        this->location = loc;
        this->name = std::move(n);
    }

    bool isDefinition() const { return !fields.empty(); }
};

/**
 * @brief 枚举常量
 */
struct EnumConstantDecl : Decl {
    ExprPtr value;          ///< 可能为 nullptr

    EnumConstantDecl(SourceLocation loc, std::string n, ExprPtr v = nullptr)
        : Decl() {
        this->location = loc;
        this->name = std::move(n);
        value = std::move(v);
    }
};

// EnumType::clone() 的实现（需要在 EnumConstantDecl 之后）
inline TypePtr EnumType::clone() const {
    auto copy = std::make_unique<EnumType>(name);
    copy->qualifiers = qualifiers;
    // 克隆常量（对于匿名枚举，常量信息必须保留）
    for (const auto& constant : constants) {
        if (constant) {
            auto constCopy = std::make_unique<EnumConstantDecl>(
                constant->location,
                constant->name,
                nullptr  // value 通常不需要克隆
            );
            copy->constants.push_back(std::move(constCopy));
        }
    }
    return copy;
}

/**
 * @brief 枚举声明
 */
struct EnumDecl : Decl {
    std::vector<std::unique_ptr<EnumConstantDecl>> constants;

    EnumDecl(SourceLocation loc, std::string n = "")
        : Decl() {
        this->location = loc;
        this->name = std::move(n);
    }
};

/**
 * @brief typedef 声明
 */
struct TypedefDecl : Decl {
    TypePtr underlyingType;

    TypedefDecl(SourceLocation loc, std::string n, TypePtr t)
        : Decl() {
        this->location = loc;
        this->name = std::move(n);
        underlyingType = std::move(t);
    }
};

//=============================================================================
// 翻译单元
//=============================================================================

/**
 * @brief 翻译单元（顶层）
 */
struct TranslationUnit : Node {
    DeclList declarations;

    TranslationUnit() = default;
};

//=============================================================================
// 访问者模式
//=============================================================================

/**
 * @brief AST 访问者基类
 */
class AstVisitor {
public:
    virtual ~AstVisitor() = default;

    // 表达式
    virtual void visit(IntLiteral&) {}
    virtual void visit(FloatLiteral&) {}
    virtual void visit(CharLiteral&) {}
    virtual void visit(StringLiteral&) {}
    virtual void visit(IdentExpr&) {}
    virtual void visit(UnaryExpr&) {}
    virtual void visit(SizeofTypeExpr&) {}
    virtual void visit(BinaryExpr&) {}
    virtual void visit(ConditionalExpr&) {}
    virtual void visit(CastExpr&) {}
    virtual void visit(SubscriptExpr&) {}
    virtual void visit(CallExpr&) {}
    virtual void visit(MemberExpr&) {}
    virtual void visit(InitListExpr&) {}
    virtual void visit(DesignatedInitExpr&) {}

    // 语句
    virtual void visit(ExprStmt&) {}
    virtual void visit(CompoundStmt&) {}
    virtual void visit(IfStmt&) {}
    virtual void visit(SwitchStmt&) {}
    virtual void visit(CaseStmt&) {}
    virtual void visit(DefaultStmt&) {}
    virtual void visit(WhileStmt&) {}
    virtual void visit(DoWhileStmt&) {}
    virtual void visit(ForStmt&) {}
    virtual void visit(GotoStmt&) {}
    virtual void visit(ContinueStmt&) {}
    virtual void visit(BreakStmt&) {}
    virtual void visit(ReturnStmt&) {}
    virtual void visit(LabelStmt&) {}

    // 声明
    virtual void visit(VarDecl&) {}
    virtual void visit(ParamDecl&) {}
    virtual void visit(FunctionDecl&) {}
    virtual void visit(FieldDecl&) {}
    virtual void visit(RecordDecl&) {}
    virtual void visit(EnumConstantDecl&) {}
    virtual void visit(EnumDecl&) {}
    virtual void visit(TypedefDecl&) {}
    virtual void visit(TranslationUnit&) {}
};

/**
 * @brief AST 打印器
 */
class AstPrinter : public AstVisitor {
public:
    explicit AstPrinter(std::ostream& os = std::cout) : os_(os), indent_(0) {}
    
    void print(TranslationUnit* tu) {
        if (!tu) return;
        os_ << "TranslationUnit\n";
        ++indent_;
        for (auto& decl : tu->declarations) {
            printDecl(decl.get());
        }
        --indent_;
    }

private:
    std::ostream& os_;
    int indent_;
    
    void printIndent() { for (int i = 0; i < indent_; ++i) os_ << "  "; }
    
    void printDecl(Decl* decl) {
        if (!decl) return;
        printIndent();
        
        if (auto* func = dynamic_cast<FunctionDecl*>(decl)) {
            os_ << "FunctionDecl: " << func->name;
            if (func->returnType) os_ << " -> " << typeToString(func->returnType.get());
            os_ << "\n";
            ++indent_;
            for (auto& param : func->params) {
                printIndent();
                os_ << "ParamDecl: " << param->name;
                if (param->type) os_ << " : " << typeToString(param->type.get());
                os_ << "\n";
            }
            if (func->body) printStmt(func->body.get());
            --indent_;
        } else if (auto* var = dynamic_cast<VarDecl*>(decl)) {
            os_ << "VarDecl: " << var->name;
            if (var->type) os_ << " : " << typeToString(var->type.get());
            os_ << "\n";
            if (var->initializer) {
                ++indent_;
                printExpr(var->initializer.get());
                --indent_;
            }
        } else if (auto* rec = dynamic_cast<RecordDecl*>(decl)) {
            os_ << (rec->isUnion ? "UnionDecl: " : "StructDecl: ") << rec->name << "\n";
            ++indent_;
            for (auto& field : rec->fields) {
                printIndent();
                os_ << "FieldDecl: " << field->name;
                if (field->type) os_ << " : " << typeToString(field->type.get());
                os_ << "\n";
            }
            --indent_;
        } else if (auto* en = dynamic_cast<EnumDecl*>(decl)) {
            os_ << "EnumDecl: " << en->name << "\n";
            ++indent_;
            for (auto& c : en->constants) {
                printIndent();
                os_ << "EnumConstant: " << c->name << "\n";
            }
            --indent_;
        } else if (auto* td = dynamic_cast<TypedefDecl*>(decl)) {
            os_ << "TypedefDecl: " << td->name;
            if (td->underlyingType) os_ << " = " << typeToString(td->underlyingType.get());
            os_ << "\n";
        } else {
            os_ << "Decl: " << decl->name << "\n";
        }
    }
    
    void printStmt(Stmt* stmt) {
        if (!stmt) return;
        printIndent();
        
        if (auto* compound = dynamic_cast<CompoundStmt*>(stmt)) {
            os_ << "CompoundStmt\n";
            ++indent_;
            for (auto& item : compound->items) {
                if (auto* s = std::get_if<StmtPtr>(&item)) printStmt(s->get());
                else if (auto* d = std::get_if<DeclPtr>(&item)) printDecl(d->get());
            }
            --indent_;
        } else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            os_ << "IfStmt\n";
            ++indent_;
            printIndent(); os_ << "Condition:\n"; ++indent_; printExpr(ifStmt->condition.get()); --indent_;
            printIndent(); os_ << "Then:\n"; ++indent_; printStmt(ifStmt->thenStmt.get()); --indent_;
            if (ifStmt->elseStmt) { printIndent(); os_ << "Else:\n"; ++indent_; printStmt(ifStmt->elseStmt.get()); --indent_; }
            --indent_;
        } else if (auto* whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            os_ << "WhileStmt\n";
            ++indent_;
            printExpr(whileStmt->condition.get());
            printStmt(whileStmt->body.get());
            --indent_;
        } else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt)) {
            os_ << "ForStmt\n";
            ++indent_; printStmt(forStmt->body.get()); --indent_;
        } else if (auto* retStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            os_ << "ReturnStmt\n";
            if (retStmt->value) { ++indent_; printExpr(retStmt->value.get()); --indent_; }
        } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
            os_ << "ExprStmt\n";
            if (exprStmt->expr) { ++indent_; printExpr(exprStmt->expr.get()); --indent_; }
        } else if (dynamic_cast<BreakStmt*>(stmt)) {
            os_ << "BreakStmt\n";
        } else if (dynamic_cast<ContinueStmt*>(stmt)) {
            os_ << "ContinueStmt\n";
        } else if (auto* switchStmt = dynamic_cast<SwitchStmt*>(stmt)) {
            os_ << "SwitchStmt\n";
            ++indent_; printExpr(switchStmt->condition.get()); printStmt(switchStmt->body.get()); --indent_;
        } else if (auto* caseStmt = dynamic_cast<CaseStmt*>(stmt)) {
            os_ << "CaseStmt\n";
            ++indent_; printExpr(caseStmt->value.get()); if (caseStmt->stmt) printStmt(caseStmt->stmt.get()); --indent_;
        } else if (auto* defStmt = dynamic_cast<DefaultStmt*>(stmt)) {
            os_ << "DefaultStmt\n";
            if (defStmt->stmt) { ++indent_; printStmt(defStmt->stmt.get()); --indent_; }
        } else if (auto* lblStmt = dynamic_cast<LabelStmt*>(stmt)) {
            os_ << "LabelStmt: " << lblStmt->label << "\n";
            if (lblStmt->stmt) { ++indent_; printStmt(lblStmt->stmt.get()); --indent_; }
        } else if (auto* gotoStmt = dynamic_cast<GotoStmt*>(stmt)) {
            os_ << "GotoStmt: " << gotoStmt->label << "\n";
        } else if (auto* doStmt = dynamic_cast<DoWhileStmt*>(stmt)) {
            os_ << "DoWhileStmt\n";
            ++indent_; printStmt(doStmt->body.get()); printExpr(doStmt->condition.get()); --indent_;
        } else {
            os_ << "Stmt\n";
        }
    }
    
    void printExpr(Expr* expr) {
        if (!expr) return;
        printIndent();
        
        if (auto* intLit = dynamic_cast<IntLiteral*>(expr)) {
            os_ << "IntLiteral: " << intLit->value << "\n";
        } else if (auto* floatLit = dynamic_cast<FloatLiteral*>(expr)) {
            os_ << "FloatLiteral: " << floatLit->value << "\n";
        } else if (auto* charLit = dynamic_cast<CharLiteral*>(expr)) {
            os_ << "CharLiteral: '" << charLit->value << "'\n";
        } else if (auto* strLit = dynamic_cast<StringLiteral*>(expr)) {
            os_ << "StringLiteral: \"" << strLit->value << "\"\n";
        } else if (auto* ident = dynamic_cast<IdentExpr*>(expr)) {
            os_ << "IdentExpr: " << ident->name << "\n";
        } else if (auto* binExpr = dynamic_cast<BinaryExpr*>(expr)) {
            os_ << "BinaryExpr: " << binaryOpToString(binExpr->op) << "\n";
            ++indent_; printExpr(binExpr->left.get()); printExpr(binExpr->right.get()); --indent_;
        } else if (auto* unaryExpr = dynamic_cast<UnaryExpr*>(expr)) {
            os_ << "UnaryExpr: " << unaryOpToString(unaryExpr->op) << "\n";
            ++indent_; printExpr(unaryExpr->operand.get()); --indent_;
        } else if (auto* callExpr = dynamic_cast<CallExpr*>(expr)) {
            os_ << "CallExpr\n";
            ++indent_;
            printExpr(callExpr->callee.get());
            for (auto& arg : callExpr->arguments) printExpr(arg.get());
            --indent_;
        } else if (auto* memberExpr = dynamic_cast<MemberExpr*>(expr)) {
            os_ << "MemberExpr: " << (memberExpr->isArrow ? "->" : ".") << memberExpr->member << "\n";
            ++indent_; printExpr(memberExpr->object.get()); --indent_;
        } else if (auto* subsExpr = dynamic_cast<SubscriptExpr*>(expr)) {
            os_ << "SubscriptExpr\n";
            ++indent_; printExpr(subsExpr->array.get()); printExpr(subsExpr->index.get()); --indent_;
        } else if (auto* castExpr = dynamic_cast<CastExpr*>(expr)) {
            os_ << "CastExpr";
            if (castExpr->targetType) os_ << ": " << typeToString(castExpr->targetType.get());
            os_ << "\n";
            ++indent_; printExpr(castExpr->operand.get()); --indent_;
        } else if (auto* condExpr = dynamic_cast<ConditionalExpr*>(expr)) {
            os_ << "ConditionalExpr\n";
            ++indent_;
            printExpr(condExpr->condition.get());
            printExpr(condExpr->thenExpr.get());
            printExpr(condExpr->elseExpr.get());
            --indent_;
        } else if (auto* initList = dynamic_cast<InitListExpr*>(expr)) {
            os_ << "InitListExpr\n";
            ++indent_;
            for (auto& elem : initList->elements) printExpr(elem.get());
            --indent_;
        } else if (auto* sizeofExpr = dynamic_cast<SizeofTypeExpr*>(expr)) {
            os_ << "SizeofTypeExpr";
            if (sizeofExpr->sizedType) os_ << ": " << typeToString(sizeofExpr->sizedType.get());
            os_ << "\n";
        } else {
            os_ << "Expr\n";
        }
    }
    
    static std::string typeToString(Type* type) {
        if (!type) return "?";
        if (auto* basic = dynamic_cast<BasicType*>(type)) {
            switch (basic->kind) {
                case BasicTypeKind::Void: return "void";
                case BasicTypeKind::Char: return "char";
                case BasicTypeKind::SChar: return "signed char";
                case BasicTypeKind::UChar: return "unsigned char";
                case BasicTypeKind::Short: return "short";
                case BasicTypeKind::UShort: return "unsigned short";
                case BasicTypeKind::Int: return "int";
                case BasicTypeKind::UInt: return "unsigned int";
                case BasicTypeKind::Long: return "long";
                case BasicTypeKind::ULong: return "unsigned long";
                case BasicTypeKind::LongLong: return "long long";
                case BasicTypeKind::ULongLong: return "unsigned long long";
                case BasicTypeKind::Float: return "float";
                case BasicTypeKind::Double: return "double";
                case BasicTypeKind::LongDouble: return "long double";
            }
        } else if (auto* ptr = dynamic_cast<PointerType*>(type)) {
            return typeToString(ptr->pointee.get()) + "*";
        } else if (auto* arr = dynamic_cast<ArrayType*>(type)) {
            return typeToString(arr->elementType.get()) + "[]";
        } else if (auto* rec = dynamic_cast<RecordType*>(type)) {
            return (rec->isUnion ? "union " : "struct ") + rec->name;
        } else if (auto* en = dynamic_cast<EnumType*>(type)) {
            return "enum " + en->name;
        } else if (dynamic_cast<FunctionType*>(type)) {
            return "function";
        }
        return "type";
    }
    
    static const char* binaryOpToString(BinaryOp op) {
        switch (op) {
            case BinaryOp::Add: return "+"; case BinaryOp::Sub: return "-";
            case BinaryOp::Mul: return "*"; case BinaryOp::Div: return "/"; case BinaryOp::Mod: return "%";
            case BinaryOp::BitAnd: return "&"; case BinaryOp::BitOr: return "|"; case BinaryOp::BitXor: return "^";
            case BinaryOp::Shl: return "<<"; case BinaryOp::Shr: return ">>";
            case BinaryOp::Eq: return "=="; case BinaryOp::Ne: return "!=";
            case BinaryOp::Lt: return "<"; case BinaryOp::Le: return "<=";
            case BinaryOp::Gt: return ">"; case BinaryOp::Ge: return ">=";
            case BinaryOp::LogAnd: return "&&"; case BinaryOp::LogOr: return "||";
            case BinaryOp::Assign: return "="; case BinaryOp::Comma: return ",";
            default: return "?";
        }
    }
    
    static const char* unaryOpToString(UnaryOp op) {
        switch (op) {
            case UnaryOp::Plus: return "+"; case UnaryOp::Minus: return "-";
            case UnaryOp::Not: return "!"; case UnaryOp::BitNot: return "~";
            case UnaryOp::PreInc: return "++pre"; case UnaryOp::PreDec: return "--pre";
            case UnaryOp::PostInc: return "post++"; case UnaryOp::PostDec: return "post--";
            case UnaryOp::Deref: return "*"; case UnaryOp::AddrOf: return "&";
            case UnaryOp::Sizeof: return "sizeof";
        }
        return "?";
    }
};

} // namespace ast
} // namespace cdd

#endif // CDD_AST_NEW_H
