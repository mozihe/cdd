/**
 * @file SymbolTable.h
 * @brief 符号表
 * @author mozihe
 * 
 * 实现多级作用域的符号表，管理变量、函数、类型等符号。
 */

#ifndef CDD_SEMANTIC_SYMBOL_TABLE_H
#define CDD_SEMANTIC_SYMBOL_TABLE_H

#include "Type.h"
#include "SourceLocation.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace cdd {
namespace semantic {

/**
 * @enum SymbolKind
 * @brief 符号类型枚举
 */
enum class SymbolKind {
    Variable,     ///< 变量
    Function,     ///< 函数
    Parameter,    ///< 函数参数
    TypeDef,      ///< typedef 名称
    StructTag,    ///< 结构体标签
    UnionTag,     ///< 联合体标签
    EnumTag,      ///< 枚举标签
    EnumConstant, ///< 枚举常量
    Label,        ///< 标签
};

/**
 * @enum StorageClass
 * @brief 存储类说明符
 */
enum class StorageClass {
    None,
    Static,
    Extern,
    Register,
    Auto,
};

/**
 * @class Symbol
 * @brief 符号表条目
 * 
 * 存储符号的名称、类型、存储类和运行时信息。
 */
class Symbol {
public:
    std::string name;          ///< 符号名称
    SymbolKind kind;           ///< 符号类型
    TypePtr type;              ///< 数据类型
    StorageClass storage = StorageClass::None;
    SourceLocation location;   ///< 定义位置

    int stackOffset = 0;       ///< 局部变量栈偏移
    std::string globalLabel;   ///< 全局符号标签
    bool isDefined = false;    ///< 是否已定义

    Symbol(const std::string& n, SymbolKind k, TypePtr t, SourceLocation loc)
        : name(n), kind(k), type(std::move(t)), location(loc) {}
};

using SymbolPtr = std::shared_ptr<Symbol>;

/**
 * @enum ScopeKind
 * @brief 作用域类型
 */
enum class ScopeKind {
    Global,   ///< 全局作用域
    Function, ///< 函数作用域
    Block,    ///< 块作用域
    Struct,   ///< 结构体作用域
};

/**
 * @class Scope
 * @brief 作用域
 * 
 * 表示一个词法作用域，包含该作用域内定义的所有符号。
 */
class Scope {
public:
    ScopeKind kind;
    Scope* parent = nullptr;
    std::unordered_map<std::string, SymbolPtr> symbols;
    int id = -1;
    
    std::string functionName;      ///< 函数名（函数作用域）
    TypePtr returnType;            ///< 返回类型（函数作用域）
    int nextLocalOffset = 0;       ///< 下一个局部变量偏移

    explicit Scope(ScopeKind k, Scope* p = nullptr)
        : kind(k), parent(p) {}

    /**
     * @brief 在当前作用域查找符号（不递归）
     * @param name 符号名
     * @return 符号指针，未找到返回 nullptr
     */
    SymbolPtr lookupLocal(const std::string& name) const {
        auto it = symbols.find(name);
        return (it != symbols.end()) ? it->second : nullptr;
    }

    /**
     * @brief 添加符号到当前作用域
     * @param sym 符号指针
     * @return 添加成功返回 true，重复定义返回 false
     */
    bool addSymbol(SymbolPtr sym) {
        if (lookupLocal(sym->name)) return false;
        symbols[sym->name] = sym;
        return true;
    }
};

/**
 * @class SymbolTable
 * @brief 符号表管理器
 * 
 * 管理所有作用域，提供符号的添加、查找和作用域管理。
 */
class SymbolTable {
public:
    SymbolTable();
    ~SymbolTable();

    /**
     * @brief 进入新作用域
     * @param kind 作用域类型
     */
    int enterScope(ScopeKind kind);
    
    /**
     * @brief 退出当前作用域
     */
    void exitScope();
    
    /**
     * @brief 获取当前作用域
     * @return 当前作用域指针
     */
    Scope* currentScope() { return currentScope_; }
    const Scope* currentScope() const { return currentScope_; }
    
    /**
     * @brief 获取全局作用域
     * @return 全局作用域指针
     */
    Scope* globalScope() { return globalScope_; }
    
    /**
     * @brief 检查是否在全局作用域
     * @return 在全局作用域返回 true
     */
    bool isGlobalScope() const { return currentScope_ == globalScope_; }
    
    /**
     * @brief 获取当前作用域 ID
     */
    int currentScopeId() const { return currentScope_ ? currentScope_->id : -1; }

    /**
     * @brief 添加符号
     * @param sym 符号指针
     * @return 添加成功返回 true
     */
    bool addSymbol(SymbolPtr sym);
    
    /**
     * @brief 递归查找符号
     * @param name 符号名
     * @return 符号指针，未找到返回 nullptr
     */
    SymbolPtr lookup(const std::string& name) const;
    
    /**
     * @brief 仅在当前作用域查找
     * @param name 符号名
     * @return 符号指针，未找到返回 nullptr
     */
    SymbolPtr lookupLocal(const std::string& name) const;

    /**
     * @brief 查找类型标签（struct/union/enum）
     * @param name 标签名
     * @return 符号指针
     */
    SymbolPtr lookupTag(const std::string& name) const;
    
    /**
     * @brief 添加类型标签
     * @param sym 符号指针
     * @return 添加成功返回 true
     */
    bool addTag(SymbolPtr sym);

    /**
     * @brief 获取所有类型标签
     * @return 标签映射表的常量引用
     */
    const std::unordered_map<std::string, SymbolPtr>& getAllTags() const { return tags_; }

    /**
     * @brief 设置当前函数信息
     * @param name 函数名
     * @param returnType 返回类型
     */
    void setCurrentFunctionInfo(const std::string& name, TypePtr returnType);
    
    /**
     * @brief 获取当前函数返回类型
     * @return 返回类型
     */
    TypePtr getCurrentReturnType() const;
    
    /**
     * @brief 分配局部变量栈空间
     * @param size 需要的字节数
     * @param alignment 对齐要求
     * @return 分配的栈偏移（负值）
     */
    int allocateLocal(int size, int alignment);
    
    /**
     * @brief 获取当前函数栈帧大小
     * @return 栈帧大小
     */
    int getCurrentStackSize() const;
    
    /**
     * @brief 通过 ID 获取作用域
     */
    Scope* getScopeById(int id) const;
    
    /**
     * @brief 切换到指定作用域 ID
     * @return 之前的作用域指针
     */
    Scope* setCurrentScopeById(int id);
    
    /**
     * @brief 直接切换到指定作用域
     * @return 之前的作用域指针
     */
    Scope* setCurrentScope(Scope* scope);

private:
    Scope* globalScope_;
    Scope* currentScope_;
    std::vector<std::unique_ptr<Scope>> allScopes_;
    int nextScopeId_ = 0;
    std::unordered_map<int, Scope*> scopeIndex_;
    std::unordered_map<std::string, SymbolPtr> tags_;
};

} // namespace semantic
} // namespace cdd

#endif // CDD_SEMANTIC_SYMBOL_TABLE_H
