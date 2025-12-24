/**
 * @file SymbolTable.cpp
 * @brief 符号表实现
 * @author mozihe
 * 
 * 实现多级作用域的符号表管理：
 * - 作用域的进入和退出
 * - 符号的添加和查找
 * - 局部变量的栈空间分配
 */

#include "SymbolTable.h"

namespace cdd {
namespace semantic {

/** @brief 构造符号表，创建全局作用域 */
SymbolTable::SymbolTable() {
    auto global = std::make_unique<Scope>(ScopeKind::Global);
    global->id = nextScopeId_++;
    globalScope_ = global.get();
    currentScope_ = globalScope_;
    scopeIndex_[global->id] = globalScope_;
    allScopes_.push_back(std::move(global));
}

SymbolTable::~SymbolTable() = default;

int SymbolTable::enterScope(ScopeKind kind) {
    auto scope = std::make_unique<Scope>(kind, currentScope_);
    scope->id = nextScopeId_++;
    scopeIndex_[scope->id] = scope.get();
    currentScope_ = scope.get();
    allScopes_.push_back(std::move(scope));
    return currentScope_->id;
}

void SymbolTable::exitScope() {
    if (currentScope_ != globalScope_) {
        currentScope_ = currentScope_->parent;
    }
}

/**
 * @brief 添加符号到当前作用域
 * 
 * 处理流程：
 * 1. 尝试添加到当前作用域（检查重复）
 * 2. 如果是局部变量，分配栈空间
 */
bool SymbolTable::addSymbol(SymbolPtr sym) {
    // 1. 添加到当前作用域
    if (!currentScope_->addSymbol(sym)) {
        return false;
    }
    
    // 2. 为局部变量分配栈空间
    if (!isGlobalScope() && 
        (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter)) {
        int size = sym->type->size();
        int align = sym->type->alignment();
        sym->stackOffset = allocateLocal(size, align);
    }
    
    return true;
}

/**
 * @brief 递归查找符号（从当前作用域向上查找）
 */
SymbolPtr SymbolTable::lookup(const std::string& name) const {
    Scope* scope = currentScope_;
    while (scope) {
        if (auto sym = scope->lookupLocal(name)) {
            return sym;
        }
        scope = scope->parent;
    }
    return nullptr;
}

SymbolPtr SymbolTable::lookupLocal(const std::string& name) const {
    return currentScope_->lookupLocal(name);
}

SymbolPtr SymbolTable::lookupTag(const std::string& name) const {
    auto it = tags_.find(name);
    return (it != tags_.end()) ? it->second : nullptr;
}

bool SymbolTable::addTag(SymbolPtr sym) {
    if (tags_.find(sym->name) != tags_.end()) {
        // 允许重复声明未完成的类型
        auto existing = tags_[sym->name];
        if (sym->kind == SymbolKind::StructTag) {
            auto existStruct = std::dynamic_pointer_cast<StructType>(existing->type);
            auto newStruct = std::dynamic_pointer_cast<StructType>(sym->type);
            if (existStruct && newStruct && !existStruct->isComplete && newStruct->isComplete) {
                // 用完整定义替换不完整定义
                tags_[sym->name] = sym;
                return true;
            }
        }
        return false;
    }
    tags_[sym->name] = sym;
    return true;
}

void SymbolTable::setCurrentFunctionInfo(const std::string& name, TypePtr returnType) {
    // 查找当前函数作用域
    Scope* scope = currentScope_;
    while (scope && scope->kind != ScopeKind::Function) {
        scope = scope->parent;
    }
    if (scope) {
        scope->functionName = name;
        scope->returnType = std::move(returnType);
    }
}

TypePtr SymbolTable::getCurrentReturnType() const {
    Scope* scope = currentScope_;
    while (scope) {
        if (scope->kind == ScopeKind::Function) {
            return scope->returnType;
        }
        scope = scope->parent;
    }
    return nullptr;
}

/**
 * @brief 分配局部变量栈空间
 * 
 * 处理流程：
 * 1. 查找所属函数作用域
 * 2. 按对齐要求调整偏移
 * 3. 分配空间并返回负偏移（相对于 RBP）
 */
int SymbolTable::allocateLocal(int size, int alignment) {
    // 1. 查找函数作用域
    Scope* funcScope = currentScope_;
    while (funcScope && funcScope->kind != ScopeKind::Function) {
        funcScope = funcScope->parent;
    }
    
    if (!funcScope) return 0;
    
    // 2. 按对齐要求调整偏移
    int& offset = funcScope->nextLocalOffset;
    offset = (offset + alignment - 1) / alignment * alignment;
    offset += size;
    
    // 3. 返回负偏移（栈向下增长）
    return -offset;
}

int SymbolTable::getCurrentStackSize() const {
    Scope* funcScope = currentScope_;
    while (funcScope && funcScope->kind != ScopeKind::Function) {
        funcScope = funcScope->parent;
    }
    
    if (!funcScope) return 0;
    
    // 对齐到 16 字节
    int size = funcScope->nextLocalOffset;
    return (size + 15) / 16 * 16;
}

Scope* SymbolTable::getScopeById(int id) const {
    auto it = scopeIndex_.find(id);
    return (it != scopeIndex_.end()) ? it->second : nullptr;
}

Scope* SymbolTable::setCurrentScopeById(int id) {
    Scope* prev = currentScope_;
    if (auto scope = getScopeById(id)) {
        currentScope_ = scope;
    }
    return prev;
}

Scope* SymbolTable::setCurrentScope(Scope* scope) {
    Scope* prev = currentScope_;
    if (scope) {
        currentScope_ = scope;
    }
    return prev;
}

} // namespace semantic
} // namespace cdd
