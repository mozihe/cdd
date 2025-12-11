/**
 * @file IRGenerator.cpp
 * @brief 中间代码生成器实现
 * @author mozihe
 */

#include "IRGenerator.h"
#include <sstream>

namespace cdd {
namespace semantic {

Operand Operand::temp(const std::string& name, TypePtr type) {
    Operand op;
    op.kind = OperandKind::Temp;
    op.name = name;
    op.type = std::move(type);
    return op;
}

Operand Operand::variable(const std::string& name, TypePtr type) {
    Operand op;
    op.kind = OperandKind::Variable;
    op.name = name;
    op.type = std::move(type);
    return op;
}

Operand Operand::intConst(int64_t value, TypePtr type) {
    Operand op;
    op.kind = OperandKind::IntConst;
    op.intValue = value;
    op.type = type ? std::move(type) : makeInt();
    return op;
}

Operand Operand::floatConst(double value, TypePtr type) {
    Operand op;
    op.kind = OperandKind::FloatConst;
    op.floatValue = value;
    op.type = type ? std::move(type) : makeDouble();
    return op;
}

Operand Operand::stringConst(const std::string& value) {
    Operand op;
    op.kind = OperandKind::StringConst;
    op.name = value;
    op.type = makePointer(makeChar());
    return op;
}

Operand Operand::label(const std::string& name) {
    Operand op;
    op.kind = OperandKind::Label;
    op.name = name;
    return op;
}

Operand Operand::global(const std::string& name, TypePtr type) {
    Operand op;
    op.kind = OperandKind::Global;
    op.name = name;
    op.type = std::move(type);
    return op;
}

bool Operand::isConst() const {
    return kind == OperandKind::IntConst || 
           kind == OperandKind::FloatConst ||
           kind == OperandKind::StringConst;
}

std::string Operand::toString() const {
    switch (kind) {
        case OperandKind::None: return "_";
        case OperandKind::Temp:
        case OperandKind::Variable:
        case OperandKind::Global:
        case OperandKind::Label: return name;
        case OperandKind::IntConst: return std::to_string(intValue);
        case OperandKind::FloatConst: return std::to_string(floatValue);
        case OperandKind::StringConst: return "\"" + name + "\"";
    }
    return "?";
}

//=============================================================================
// Quadruple 实现
//=============================================================================

const char* opcodeToString(IROpcode op) {
    switch (op) {
        case IROpcode::Add: return "ADD";
        case IROpcode::Sub: return "SUB";
        case IROpcode::Mul: return "MUL";
        case IROpcode::Div: return "DIV";
        case IROpcode::Mod: return "MOD";
        case IROpcode::Neg: return "NEG";
        case IROpcode::BitAnd: return "AND";
        case IROpcode::BitOr: return "OR";
        case IROpcode::BitXor: return "XOR";
        case IROpcode::BitNot: return "NOT";
        case IROpcode::Shl: return "SHL";
        case IROpcode::Shr: return "SHR";
        case IROpcode::Eq: return "EQ";
        case IROpcode::Ne: return "NE";
        case IROpcode::Lt: return "LT";
        case IROpcode::Le: return "LE";
        case IROpcode::Gt: return "GT";
        case IROpcode::Ge: return "GE";
        case IROpcode::LogicalAnd: return "LAND";
        case IROpcode::LogicalOr: return "LOR";
        case IROpcode::LogicalNot: return "LNOT";
        case IROpcode::Assign: return "MOV";
        case IROpcode::Load: return "LOAD";
        case IROpcode::Store: return "STORE";
        case IROpcode::LoadAddr: return "LEA";
        case IROpcode::IndexAddr: return "INDEX";
        case IROpcode::MemberAddr: return "MEMBER";
        case IROpcode::Label: return "LABEL";
        case IROpcode::Jump: return "JMP";
        case IROpcode::JumpTrue: return "JT";
        case IROpcode::JumpFalse: return "JF";
        case IROpcode::Param: return "PARAM";
        case IROpcode::Call: return "CALL";
        case IROpcode::Return: return "RET";
        case IROpcode::IntToFloat: return "I2F";
        case IROpcode::FloatToInt: return "F2I";
        case IROpcode::IntExtend: return "EXT";
        case IROpcode::IntTrunc: return "TRUNC";
        case IROpcode::PtrToInt: return "P2I";
        case IROpcode::IntToPtr: return "I2P";
        case IROpcode::Switch: return "SWITCH";
        case IROpcode::Case: return "CASE";
        case IROpcode::Nop: return "NOP";
        case IROpcode::Comment: return "//";
    }
    return "???";
}

std::string Quadruple::toString() const {
    std::ostringstream oss;
    
    if (opcode == IROpcode::Label) {
        oss << result.name << ":";
        return oss.str();
    }
    
    oss << "  " << opcodeToString(opcode);
    if (!result.isNone()) oss << " " << result.toString();
    if (!arg1.isNone()) oss << ", " << arg1.toString();
    if (!arg2.isNone()) oss << ", " << arg2.toString();
    
    return oss.str();
}

//=============================================================================
// IRGenerator 实现
//=============================================================================

IRGenerator::IRGenerator(SymbolTable& symTable) : symTable_(symTable) {}
IRGenerator::~IRGenerator() = default;

std::string IRGenerator::newTemp() {
    return "t" + std::to_string(tempCounter_++);
}

std::string IRGenerator::newLabel(const std::string& prefix) {
    return prefix + std::to_string(labelCounter_++);
}

std::string IRGenerator::addStringLiteral(const std::string& value) {
    std::string label = ".LC" + std::to_string(stringCounter_++);
    program_.stringLiterals.push_back({label, value});
    return label;
}

void IRGenerator::emit(IROpcode op, Operand result, Operand arg1, Operand arg2) {
    if (currentFunc_) {
        currentFunc_->code.push_back(Quadruple(op, result, arg1, arg2));
    }
}

void IRGenerator::emitLabel(const std::string& label) {
    emit(IROpcode::Label, Operand::label(label));
}

void IRGenerator::emitJump(const std::string& label) {
    emit(IROpcode::Jump, Operand::label(label));
}

void IRGenerator::emitJumpTrue(const Operand& cond, const std::string& label) {
    emit(IROpcode::JumpTrue, Operand::label(label), cond);
}

void IRGenerator::emitJumpFalse(const Operand& cond, const std::string& label) {
    emit(IROpcode::JumpFalse, Operand::label(label), cond);
}

void IRGenerator::emitComment(const std::string& comment) {
    emit(IROpcode::Comment, Operand::label(comment));
}

//=============================================================================
// 程序生成入口
//=============================================================================

/**
 * @brief 生成整个程序的 IR
 * 
 * 遍历翻译单元中的所有声明，生成对应的 IR 代码。
 */
IRProgram IRGenerator::generate(ast::TranslationUnit* unit) {
    if (!unit) return program_;
    
    for (auto& decl : unit->declarations) {
        genDecl(decl.get());
    }
    
    return std::move(program_);
}

//=============================================================================
// 声明处理
//=============================================================================

void IRGenerator::genDecl(ast::Decl* decl) {
    if (!decl) return;

    if (auto varDecl = dynamic_cast<ast::VarDecl*>(decl)) {
        if (symTable_.isGlobalScope()) {
            genGlobalVar(varDecl);
        } else {
            genVarDecl(varDecl);
        }
    } else if (auto funcDecl = dynamic_cast<ast::FunctionDecl*>(decl)) {
        genFunctionDecl(funcDecl);
    } else if (auto typedefDecl = dynamic_cast<ast::TypedefDecl*>(decl)) {
        // 添加 typedef 到符号表
        TypePtr underlyingType = astTypeToSemType(typedefDecl->underlyingType.get());
        if (underlyingType) {
            auto sym = std::make_shared<Symbol>(typedefDecl->name, SymbolKind::TypeDef, 
                                                underlyingType, typedefDecl->location);
            symTable_.addSymbol(sym);
        }
    } else if (auto enumDecl = dynamic_cast<ast::EnumDecl*>(decl)) {
        // 处理枚举常量值
        int64_t nextValue = 0;
        for (const auto& constant : enumDecl->constants) {
            int64_t value = nextValue;
            if (constant->value) {
                if (auto intLit = dynamic_cast<ast::IntLiteral*>(constant->value.get())) {
                    value = intLit->value;
                }
            }
            // 存储枚举常量值到映射
            enumConstValues_[constant->name] = value;
            nextValue = value + 1;
        }
    }
}

void IRGenerator::genGlobalVar(ast::VarDecl* decl) {
    if (!decl) return;
    
    // 从 AST 获取类型信息
    TypePtr varType = astTypeToSemType(decl->type.get());
    if (!varType) varType = makeInt();
    
    bool isExtern = (decl->storage == ast::StorageClass::Extern);
    
    // 添加到全局符号表
    auto sym = std::make_shared<Symbol>(decl->name, SymbolKind::Variable, varType, decl->location);
    sym->storage = isExtern ? StorageClass::Extern : StorageClass::Static;
    symTable_.addSymbol(sym);
    
    GlobalVar gv;
    gv.name = decl->name;
    gv.type = varType;
    gv.hasInitializer = (decl->initializer != nullptr);
    gv.isExtern = isExtern;
    
    program_.globals.push_back(gv);
}

/**
 * @brief 生成局部变量声明的 IR
 * 
 * 处理流程：
 * 1. 解析变量类型
 * 2. 处理不完整数组类型（推导大小）
 * 3. 添加符号到符号表
 * 4. 生成初始化代码（字符串、数组、普通表达式）
 */
void IRGenerator::genVarDecl(ast::VarDecl* decl) {
    if (!decl) return;
    
    // 1. 从AST类型获取语义类型
    TypePtr varType = astTypeToSemType(decl->type.get());
    if (!varType) varType = makeInt();
    
    // 2. 特殊处理：数组类型初始化，可能需要推导大小
    if (varType->isArray() && decl->initializer) {
        auto arrType = static_cast<ArrayType*>(varType.get());
        if (arrType->length < 0) {  // 不完整数组，需要推导大小
            if (auto strLit = dynamic_cast<ast::StringLiteral*>(decl->initializer.get())) {
                // 从字符串长度推导数组大小（包括终止符）
                int inferredSize = static_cast<int>(strLit->value.size()) + 1;
                varType = makeArray(arrType->elementType, inferredSize);
            } else if (auto initList = dynamic_cast<ast::InitListExpr*>(decl->initializer.get())) {
                // 从初始化列表推导数组大小
                int inferredSize = static_cast<int>(initList->elements.size());
                varType = makeArray(arrType->elementType, inferredSize);
            }
        }
    }
    
    // 添加符号到符号表（IR生成阶段也需要）
    auto sym = std::make_shared<Symbol>(decl->name, SymbolKind::Variable, varType, decl->location);
    symTable_.addSymbol(sym);
    
    // 为局部变量生成唯一的 IR 名称（支持变量 shadowing）
    std::string irName = decl->name + "_" + std::to_string(varCounter_++);
    varIRNames_[sym.get()] = irName;
    
    if (decl->initializer) {
        // 特殊处理：数组类型初始化
        if (varType->isArray()) {
            auto arrSemType = static_cast<ArrayType*>(varType.get());
            TypePtr elemType = arrSemType->elementType;
            
            if (auto strLit = dynamic_cast<ast::StringLiteral*>(decl->initializer.get())) {
                // char arr[] = "string" - 需要复制字符串到栈上数组
                Operand arrAddr = Operand::temp(newTemp(), makePointer(elemType));
                emit(IROpcode::LoadAddr, arrAddr, Operand::variable(irName, varType));
                
                const std::string& str = strLit->value;
                for (size_t i = 0; i <= str.size(); ++i) {  // 包括终止符
                    char c = (i < str.size()) ? str[i] : '\0';
                    Operand elemAddr = Operand::temp(newTemp(), makePointer(elemType));
                    emit(IROpcode::IndexAddr, elemAddr, arrAddr, Operand::intConst(i));
                    emit(IROpcode::Store, elemAddr, Operand::intConst(c, elemType));
                }
                return;
            }
            
            if (auto initList = dynamic_cast<ast::InitListExpr*>(decl->initializer.get())) {
                // int arr[] = {1, 2, 3} - 逐个初始化数组元素
                Operand arrAddr = Operand::temp(newTemp(), makePointer(elemType));
                emit(IROpcode::LoadAddr, arrAddr, Operand::variable(irName, varType));
                
                for (size_t i = 0; i < initList->elements.size(); ++i) {
                    Operand elemVal = genExpr(initList->elements[i].get());
                    Operand elemAddr = Operand::temp(newTemp(), makePointer(elemType));
                    emit(IROpcode::IndexAddr, elemAddr, arrAddr, Operand::intConst(i));
                    emit(IROpcode::Store, elemAddr, elemVal);
                }
                return;
            }
        }
        
        Operand dest = Operand::variable(irName, varType);
        Operand src = genExpr(decl->initializer.get());
        
        // 数组衰减为指针：当指针用数组初始化时，需要获取数组地址
        if (varType->isPointer() && src.type && src.type->isArray()) {
            Operand addr = Operand::temp(newTemp(), varType);
            emit(IROpcode::LoadAddr, addr, src);
            src = addr;
        }
        
        emit(IROpcode::Assign, dest, src);
    }
}

/**
 * @brief 生成函数定义的 IR
 * 
 * 处理流程：
 * 1. 解析返回类型和参数类型
 * 2. 创建函数符号并添加到符号表
 * 3. 如果只是声明（无函数体），直接返回
 * 4. 创建函数作用域，添加参数符号
 * 5. 生成函数体 IR
 * 6. 添加隐式返回（如果需要）
 * 7. 保存函数 IR 到程序
 */
void IRGenerator::genFunctionDecl(ast::FunctionDecl* decl) {
    if (!decl) return;
    
    // 1. 解析返回类型
    TypePtr returnType = astTypeToSemType(decl->returnType.get());
    if (!returnType) returnType = makeInt();
    
    // 2. 创建函数类型并添加到符号表
    auto funcType = std::make_shared<FunctionType>(returnType);
    funcType->isVariadic = decl->isVariadic;
    for (const auto& param : decl->params) {
        TypePtr paramType = astTypeToSemType(param->type.get());
        if (!paramType) paramType = makeInt();
        funcType->paramTypes.push_back(paramType);
        funcType->paramNames.push_back(param->name);
    }
    
    auto funcSym = std::make_shared<Symbol>(decl->name, SymbolKind::Function, funcType, decl->location);
    funcSym->globalLabel = decl->name;
    symTable_.addSymbol(funcSym);
    
    // 3. 如果只是声明，返回
    if (!decl->body) return;
    
    // 4. 创建函数 IR 结构
    FunctionIR func;
    func.name = decl->name;
    func.returnType = returnType;
    func.isVariadic = decl->isVariadic;
    
    for (const auto& param : decl->params) {
        TypePtr paramType = astTypeToSemType(param->type.get());
        if (!paramType) paramType = makeInt();
        func.parameters.push_back({param->name, paramType});
    }
    
    currentFunc_ = &func;
    tempCounter_ = 0;
    
    // 进入函数作用域
    symTable_.enterScope(ScopeKind::Function);
    symTable_.setCurrentFunctionInfo(decl->name, func.returnType);
    
    // 添加参数到符号表
    for (const auto& param : decl->params) {
        if (!param->name.empty()) {
            TypePtr paramType = astTypeToSemType(param->type.get());
            if (!paramType) paramType = makeInt();
            auto paramSym = std::make_shared<Symbol>(
                param->name, SymbolKind::Parameter, paramType, param->location);
            symTable_.addSymbol(paramSym);
            varIRNames_[paramSym.get()] = param->name;
        }
    }
    
    // 5. 生成函数体
    genCompoundStmt(decl->body.get());
    
    // 6. 添加隐式返回
    if (currentFunc_->code.empty() || 
        currentFunc_->code.back().opcode != IROpcode::Return) {
        emit(IROpcode::Return);
    }
    
    // 7. 保存函数 IR
    func.stackSize = symTable_.getCurrentStackSize();
    symTable_.exitScope();
    
    program_.functions.push_back(std::move(func));
    currentFunc_ = nullptr;
}

//=============================================================================
// 语句处理
//=============================================================================

void IRGenerator::genStmt(ast::Stmt* stmt) {
    if (!stmt) return;
    
    if (auto exprStmt = dynamic_cast<ast::ExprStmt*>(stmt)) {
        if (exprStmt->expr) genExpr(exprStmt->expr.get());
    } else if (auto compoundStmt = dynamic_cast<ast::CompoundStmt*>(stmt)) {
        genCompoundStmt(compoundStmt);
    } else if (auto ifStmt = dynamic_cast<ast::IfStmt*>(stmt)) {
        genIfStmt(ifStmt);
    } else if (auto whileStmt = dynamic_cast<ast::WhileStmt*>(stmt)) {
        genWhileStmt(whileStmt);
    } else if (auto doWhileStmt = dynamic_cast<ast::DoWhileStmt*>(stmt)) {
        genDoWhileStmt(doWhileStmt);
    } else if (auto forStmt = dynamic_cast<ast::ForStmt*>(stmt)) {
        genForStmt(forStmt);
    } else if (auto switchStmt = dynamic_cast<ast::SwitchStmt*>(stmt)) {
        genSwitchStmt(switchStmt);
    } else if (auto caseStmt = dynamic_cast<ast::CaseStmt*>(stmt)) {
        genCaseStmt(caseStmt);
    } else if (auto defaultStmt = dynamic_cast<ast::DefaultStmt*>(stmt)) {
        genDefaultStmt(defaultStmt);
    } else if (auto breakStmt = dynamic_cast<ast::BreakStmt*>(stmt)) {
        genBreakStmt(breakStmt);
    } else if (auto continueStmt = dynamic_cast<ast::ContinueStmt*>(stmt)) {
        genContinueStmt(continueStmt);
    } else if (auto returnStmt = dynamic_cast<ast::ReturnStmt*>(stmt)) {
        genReturnStmt(returnStmt);
    } else if (auto labelStmt = dynamic_cast<ast::LabelStmt*>(stmt)) {
        emitLabel(labelStmt->label);
        genStmt(labelStmt->stmt.get());
    } else if (auto gotoStmt = dynamic_cast<ast::GotoStmt*>(stmt)) {
        emitJump(gotoStmt->label);
    }
}

void IRGenerator::genCompoundStmt(ast::CompoundStmt* stmt) {
    if (!stmt) return;
    
    // 复合语句创建新的块作用域
    symTable_.enterScope(ScopeKind::Block);
    
    for (auto& item : stmt->items) {
        if (std::holds_alternative<ast::DeclPtr>(item)) {
            genDecl(std::get<ast::DeclPtr>(item).get());
        } else if (std::holds_alternative<ast::StmtPtr>(item)) {
            genStmt(std::get<ast::StmtPtr>(item).get());
        }
    }
    
    symTable_.exitScope();
}

void IRGenerator::genIfStmt(ast::IfStmt* stmt) {
    if (!stmt) return;
    
    std::string elseLabel = newLabel("else");
    std::string endLabel = newLabel("endif");
    
    Operand cond = genExpr(stmt->condition.get());
    
    if (stmt->elseStmt) {
        emitJumpFalse(cond, elseLabel);
        genStmt(stmt->thenStmt.get());
        emitJump(endLabel);
        emitLabel(elseLabel);
        genStmt(stmt->elseStmt.get());
    } else {
        emitJumpFalse(cond, endLabel);
        genStmt(stmt->thenStmt.get());
    }
    
    emitLabel(endLabel);
}

void IRGenerator::genWhileStmt(ast::WhileStmt* stmt) {
    if (!stmt) return;
    
    std::string startLabel = newLabel("while");
    std::string endLabel = newLabel("endwhile");
    
    breakTargets_.push_back(endLabel);
    continueTargets_.push_back(startLabel);
    
    emitLabel(startLabel);
    Operand cond = genExpr(stmt->condition.get());
    emitJumpFalse(cond, endLabel);
    
    genStmt(stmt->body.get());
    emitJump(startLabel);
    
    emitLabel(endLabel);
    
    breakTargets_.pop_back();
    continueTargets_.pop_back();
}

void IRGenerator::genDoWhileStmt(ast::DoWhileStmt* stmt) {
    if (!stmt) return;
    
    std::string startLabel = newLabel("do");
    std::string condLabel = newLabel("docond");
    std::string endLabel = newLabel("enddo");
    
    breakTargets_.push_back(endLabel);
    continueTargets_.push_back(condLabel);
    
    emitLabel(startLabel);
    genStmt(stmt->body.get());
    
    emitLabel(condLabel);
    Operand cond = genExpr(stmt->condition.get());
    emitJumpTrue(cond, startLabel);
    
    emitLabel(endLabel);
    
    breakTargets_.pop_back();
    continueTargets_.pop_back();
}

void IRGenerator::genForStmt(ast::ForStmt* stmt) {
    if (!stmt) return;
    
    std::string condLabel = newLabel("forcond");
    std::string incLabel = newLabel("forinc");
    std::string endLabel = newLabel("endfor");
    
    // for 循环可能在初始化中声明变量，需要新作用域
    symTable_.enterScope(ScopeKind::Block);
    
    // 初始化
    if (std::holds_alternative<ast::DeclList>(stmt->init)) {
        for (auto& decl : std::get<ast::DeclList>(stmt->init)) {
            genDecl(decl.get());
        }
    } else if (std::holds_alternative<ast::StmtPtr>(stmt->init)) {
        genStmt(std::get<ast::StmtPtr>(stmt->init).get());
    }
    
    breakTargets_.push_back(endLabel);
    continueTargets_.push_back(incLabel);
    
    // 条件
    emitLabel(condLabel);
    if (stmt->condition) {
        Operand cond = genExpr(stmt->condition.get());
        emitJumpFalse(cond, endLabel);
    }
    
    // 循环体
    genStmt(stmt->body.get());
    
    // 增量
    emitLabel(incLabel);
    if (stmt->increment) genExpr(stmt->increment.get());
    emitJump(condLabel);
    
    emitLabel(endLabel);
    
    breakTargets_.pop_back();
    continueTargets_.pop_back();
    symTable_.exitScope();
}

void IRGenerator::genSwitchStmt(ast::SwitchStmt* stmt) {
    if (!stmt) return;
    
    Operand condVal = genExpr(stmt->condition.get());
    
    SwitchInfo info;
    info.condition = condVal;
    info.endLabel = newLabel("endswitch");
    info.defaultLabel = info.endLabel;
    
    switchStack_.push_back(info);
    breakTargets_.push_back(info.endLabel);
    
    // 跳过 switch 主体，先生成跳转表
    std::string bodyLabel = newLabel("switchbody");
    std::string tableLabel = newLabel("switchtable");
    emitJump(tableLabel);
    
    // 生成 switch 主体
    emitLabel(bodyLabel);
    genStmt(stmt->body.get());
    emitJump(info.endLabel);
    
    // 生成跳转表
    emitLabel(tableLabel);
    auto& switchInfo = switchStack_.back();
    for (const auto& c : switchInfo.cases) {
        // 比较并跳转
        Operand cmp = Operand::temp(newTemp(), makeInt());
        emit(IROpcode::Eq, cmp, condVal, Operand::intConst(c.first));
        emitJumpTrue(cmp, c.second);
    }
    // 跳到 default 或结束
    emitJump(switchInfo.defaultLabel);
    
    emitLabel(info.endLabel);
    
    breakTargets_.pop_back();
    switchStack_.pop_back();
}

void IRGenerator::genCaseStmt(ast::CaseStmt* stmt) {
    if (!stmt || switchStack_.empty()) return;
    
    std::string caseLabel = newLabel("case");
    emitLabel(caseLabel);
    
    // 保存标签到 stmt 供外部使用
    stmt->label = caseLabel;
    
    int64_t caseValue = 0;
    if (auto intLit = dynamic_cast<ast::IntLiteral*>(stmt->value.get())) {
        caseValue = intLit->value;
    } else if (auto charLit = dynamic_cast<ast::CharLiteral*>(stmt->value.get())) {
        caseValue = static_cast<int64_t>(charLit->value);
    }
    
    switchStack_.back().cases.push_back({caseValue, caseLabel});
    
    if (stmt->stmt) genStmt(stmt->stmt.get());
}

void IRGenerator::genDefaultStmt(ast::DefaultStmt* stmt) {
    if (!stmt || switchStack_.empty()) return;
    
    std::string defaultLabel = newLabel("default");
    emitLabel(defaultLabel);
    
    stmt->label = defaultLabel;
    switchStack_.back().defaultLabel = defaultLabel;
    
    if (stmt->stmt) genStmt(stmt->stmt.get());
}

void IRGenerator::genBreakStmt(ast::BreakStmt* stmt) {
    if (!breakTargets_.empty()) {
        emitJump(breakTargets_.back());
    }
}

void IRGenerator::genContinueStmt(ast::ContinueStmt* stmt) {
    if (!continueTargets_.empty()) {
        emitJump(continueTargets_.back());
    }
}

void IRGenerator::genReturnStmt(ast::ReturnStmt* stmt) {
    if (!stmt) return;
    
    if (stmt->value) {
        Operand retVal = genExpr(stmt->value.get());
        emit(IROpcode::Return, Operand::none(), retVal);
    } else {
        emit(IROpcode::Return);
    }
}

//=============================================================================
// 表达式处理
//=============================================================================

Operand IRGenerator::genExpr(ast::Expr* expr) {
    if (!expr) return Operand::none();
    
    if (auto intLit = dynamic_cast<ast::IntLiteral*>(expr)) {
        return genIntLiteral(intLit);
    } else if (auto floatLit = dynamic_cast<ast::FloatLiteral*>(expr)) {
        return genFloatLiteral(floatLit);
    } else if (auto charLit = dynamic_cast<ast::CharLiteral*>(expr)) {
        return genCharLiteral(charLit);
    } else if (auto strLit = dynamic_cast<ast::StringLiteral*>(expr)) {
        return genStringLiteral(strLit);
    } else if (auto identExpr = dynamic_cast<ast::IdentExpr*>(expr)) {
        return genIdentExpr(identExpr);
    } else if (auto binaryExpr = dynamic_cast<ast::BinaryExpr*>(expr)) {
        return genBinaryExpr(binaryExpr);
    } else if (auto unaryExpr = dynamic_cast<ast::UnaryExpr*>(expr)) {
        return genUnaryExpr(unaryExpr);
    } else if (auto callExpr = dynamic_cast<ast::CallExpr*>(expr)) {
        return genCallExpr(callExpr);
    } else if (auto subscriptExpr = dynamic_cast<ast::SubscriptExpr*>(expr)) {
        return genSubscriptExpr(subscriptExpr);
    } else if (auto memberExpr = dynamic_cast<ast::MemberExpr*>(expr)) {
        return genMemberExpr(memberExpr);
    } else if (auto castExpr = dynamic_cast<ast::CastExpr*>(expr)) {
        return genCastExpr(castExpr);
    } else if (auto condExpr = dynamic_cast<ast::ConditionalExpr*>(expr)) {
        return genConditionalExpr(condExpr);
    } else if (auto sizeofTypeExpr = dynamic_cast<ast::SizeofTypeExpr*>(expr)) {
        // sizeof(type) 是编译时常量
        TypePtr type = astTypeToSemType(sizeofTypeExpr->sizedType.get());
        int size = type ? type->size() : 0;
        return Operand::intConst(size, makeLong(true));
    }
    
    return Operand::none();
}

Operand IRGenerator::genIntLiteral(ast::IntLiteral* expr) {
    if (!expr) return Operand::none();
    // 使用语义分析器填充的类型信息
    TypePtr type = getExprSemType(expr);
    return Operand::intConst(expr->value, type ? type : makeInt());
}

Operand IRGenerator::genFloatLiteral(ast::FloatLiteral* expr) {
    if (!expr) return Operand::none();
    // 使用语义分析器填充的类型信息
    TypePtr type = getExprSemType(expr);
    return Operand::floatConst(expr->value, type ? type : makeDouble());
}

Operand IRGenerator::genCharLiteral(ast::CharLiteral* expr) {
    if (!expr) return Operand::none();
    return Operand::intConst(expr->value, makeChar());
}

Operand IRGenerator::genStringLiteral(ast::StringLiteral* expr) {
    if (!expr) return Operand::none();
    std::string label = addStringLiteral(expr->value);
    return Operand::global(label, makePointer(makeChar()));
}

Operand IRGenerator::genIdentExpr(ast::IdentExpr* expr) {
    if (!expr) return Operand::none();
    
    auto sym = symTable_.lookup(expr->name);
    if (!sym) return Operand::none();
    
    if (sym->kind == SymbolKind::EnumConstant) {
        // 枚举常量：从我们的映射获取值
        auto it = enumConstValues_.find(expr->name);
        if (it != enumConstValues_.end()) {
            return Operand::intConst(it->second, makeInt());
        }
        // 回退：从枚举类型获取值（如果类型是 EnumType）
        if (sym->type && sym->type->isEnum()) {
            auto enumType = static_cast<EnumType*>(sym->type.get());
            auto enumIt = enumType->enumerators.find(expr->name);
            if (enumIt != enumType->enumerators.end()) {
                return Operand::intConst(enumIt->second, makeInt());
            }
        }
        return Operand::intConst(0);
    }
    
    // 函数名：返回函数标签
    if (sym->kind == SymbolKind::Function) {
        return Operand::label(expr->name);
    }
    
    // 全局变量或静态变量
    if (symTable_.isGlobalScope() || sym->storage == StorageClass::Static || 
        sym->storage == StorageClass::Extern) {
        return Operand::global(expr->name, sym->type);
    }
    
    // 局部变量或参数：使用唯一的 IR 名称
    auto it = varIRNames_.find(sym.get());
    if (it != varIRNames_.end()) {
        return Operand::variable(it->second, sym->type);
    }
    
    // 回退到原始名称（参数等）
    return Operand::variable(expr->name, sym->type);
}

Operand IRGenerator::genBinaryExpr(ast::BinaryExpr* expr) {
    if (!expr) return Operand::none();
    
    using Op = ast::BinaryOp;
    
    // 赋值
    if (expr->op == Op::Assign) {
        return genAssignment(expr);
    }
    
    // 复合赋值
    if (expr->op >= Op::AddAssign && expr->op <= Op::ShrAssign) {
        Operand left = genLValueAddr(expr->left.get());
        Operand right = genExpr(expr->right.get());
        
        TypePtr leftType = getExprSemType(expr->left.get());
        if (!leftType) return Operand::none();
        
        Operand leftVal = Operand::temp(newTemp(), leftType);
        emit(IROpcode::Load, leftVal, left);
        
        IROpcode op;
        switch (expr->op) {
            case Op::AddAssign: op = IROpcode::Add; break;
            case Op::SubAssign: op = IROpcode::Sub; break;
            case Op::MulAssign: op = IROpcode::Mul; break;
            case Op::DivAssign: op = IROpcode::Div; break;
            case Op::ModAssign: op = IROpcode::Mod; break;
            case Op::AndAssign: op = IROpcode::BitAnd; break;
            case Op::OrAssign: op = IROpcode::BitOr; break;
            case Op::XorAssign: op = IROpcode::BitXor; break;
            case Op::ShlAssign: op = IROpcode::Shl; break;
            case Op::ShrAssign: op = IROpcode::Shr; break;
            default: op = IROpcode::Nop; break;
        }
        
        TypePtr resultType = getExprSemType(expr);
        if (!resultType) return Operand::none();
        
        Operand result = Operand::temp(newTemp(), resultType);
        emit(op, result, leftVal, right);
        emit(IROpcode::Store, left, result);
        return result;
    }
    
    // 短路逻辑
    if (expr->op == Op::LogAnd) return genLogicalAnd(expr);
    if (expr->op == Op::LogOr) return genLogicalOr(expr);
    
    // 逗号运算符
    if (expr->op == Op::Comma) {
        genExpr(expr->left.get());
        return genExpr(expr->right.get());
    }
    
    // 普通二元运算
    Operand left = genExpr(expr->left.get());
    Operand right = genExpr(expr->right.get());
    
    TypePtr leftType = getExprSemType(expr->left.get());
    TypePtr rightType = getExprSemType(expr->right.get());
    
    // 指针算术处理（数组也按指针处理，因为会衰减为指针）
    if (expr->op == Op::Add || expr->op == Op::Sub) {
        // 获取元素大小和元素类型
        auto getElemInfo = [](TypePtr type) -> std::pair<int, TypePtr> {
            if (!type) return {1, nullptr};
            if (type->isPointer()) {
                auto ptrType = static_cast<PointerType*>(type.get());
                return {ptrType->pointee ? ptrType->pointee->size() : 1, ptrType->pointee};
            } else if (type->isArray()) {
                auto arrType = static_cast<ArrayType*>(type.get());
                return {arrType->elementType ? arrType->elementType->size() : 1, arrType->elementType};
            }
            return {1, nullptr};
        };
        
        // 数组衰减为指针：如果操作数是数组类型，需要获取其地址
        if (leftType && leftType->isArray()) {
            auto [elemSize, elemType] = getElemInfo(leftType);
            Operand addr = Operand::temp(newTemp(), makePointer(elemType));
            emit(IROpcode::LoadAddr, addr, left);
            left = addr;
            leftType = makePointer(elemType);
        }
        if (rightType && rightType->isArray()) {
            auto [elemSize, elemType] = getElemInfo(rightType);
            Operand addr = Operand::temp(newTemp(), makePointer(elemType));
            emit(IROpcode::LoadAddr, addr, right);
            right = addr;
            rightType = makePointer(elemType);
        }
        
        // 在数组衰减后重新计算类型标志
        bool leftIsPtr = leftType && leftType->isPointer();
        bool rightIsPtr = rightType && rightType->isPointer();
        bool leftIsInt = leftType && leftType->isInteger();
        bool rightIsInt = rightType && rightType->isInteger();
        
        // 指针 + 整数
        if (expr->op == Op::Add && leftIsPtr && rightIsInt) {
            auto [elemSize, elemType] = getElemInfo(leftType);
            if (elemSize > 1) {
                Operand scaled = Operand::temp(newTemp(), rightType);
                emit(IROpcode::Mul, scaled, right, Operand::intConst(elemSize));
                right = scaled;
            }
            TypePtr resultType = getExprSemType(expr);
            if (!resultType) resultType = leftType;
            Operand result = Operand::temp(newTemp(), resultType);
            emit(IROpcode::Add, result, left, right);
            return result;
        }
        // 整数 + 指针
        else if (expr->op == Op::Add && rightIsPtr && leftIsInt) {
            auto [elemSize, elemType] = getElemInfo(rightType);
            if (elemSize > 1) {
                Operand scaled = Operand::temp(newTemp(), leftType);
                emit(IROpcode::Mul, scaled, left, Operand::intConst(elemSize));
                left = scaled;
            }
            TypePtr resultType = getExprSemType(expr);
            if (!resultType) resultType = rightType;
            Operand result = Operand::temp(newTemp(), resultType);
            emit(IROpcode::Add, result, left, right);
            return result;
        }
        // 指针 - 整数
        else if (expr->op == Op::Sub && leftIsPtr && rightIsInt) {
            auto [elemSize, elemType] = getElemInfo(leftType);
            if (elemSize > 1) {
                Operand scaled = Operand::temp(newTemp(), rightType);
                emit(IROpcode::Mul, scaled, right, Operand::intConst(elemSize));
                right = scaled;
            }
            TypePtr resultType = getExprSemType(expr);
            if (!resultType) resultType = leftType;
            Operand result = Operand::temp(newTemp(), resultType);
            emit(IROpcode::Sub, result, left, right);
            return result;
        }
        // 指针 - 指针：结果需要除以元素大小
        else if (expr->op == Op::Sub && leftIsPtr && rightIsPtr) {
            auto [elemSize, elemType] = getElemInfo(leftType);
            TypePtr resultType = getExprSemType(expr);
            if (!resultType) resultType = makeLong();
            Operand diff = Operand::temp(newTemp(), resultType);
            emit(IROpcode::Sub, diff, left, right);
            if (elemSize > 1) {
                Operand result = Operand::temp(newTemp(), resultType);
                emit(IROpcode::Div, result, diff, Operand::intConst(elemSize));
                return result;
            }
            return diff;
        }
    }
    
    IROpcode op;
    switch (expr->op) {
        case Op::Add: op = IROpcode::Add; break;
        case Op::Sub: op = IROpcode::Sub; break;
        case Op::Mul: op = IROpcode::Mul; break;
        case Op::Div: op = IROpcode::Div; break;
        case Op::Mod: op = IROpcode::Mod; break;
        case Op::BitAnd: op = IROpcode::BitAnd; break;
        case Op::BitOr: op = IROpcode::BitOr; break;
        case Op::BitXor: op = IROpcode::BitXor; break;
        case Op::Shl: op = IROpcode::Shl; break;
        case Op::Shr: op = IROpcode::Shr; break;
        case Op::Eq: op = IROpcode::Eq; break;
        case Op::Ne: op = IROpcode::Ne; break;
        case Op::Lt: op = IROpcode::Lt; break;
        case Op::Le: op = IROpcode::Le; break;
        case Op::Gt: op = IROpcode::Gt; break;
        case Op::Ge: op = IROpcode::Ge; break;
        default: return Operand::none();
    }
    
    TypePtr resultType = getExprSemType(expr);
    
    // 比较运算符的结果类型总是 int
    if (!resultType && (expr->op == Op::Eq || expr->op == Op::Ne || 
                        expr->op == Op::Lt || expr->op == Op::Le ||
                        expr->op == Op::Gt || expr->op == Op::Ge)) {
        resultType = makeInt();
    }
    
    // 算术运算符：如果结果类型未知，从操作数类型推断
    if (!resultType && (expr->op == Op::Add || expr->op == Op::Sub ||
                        expr->op == Op::Mul || expr->op == Op::Div ||
                        expr->op == Op::Mod || expr->op == Op::BitAnd ||
                        expr->op == Op::BitOr || expr->op == Op::BitXor ||
                        expr->op == Op::Shl || expr->op == Op::Shr)) {
        // 优先使用左操作数类型（对于 enum + int，左侧可能是 int）
        if (leftType && (leftType->isInteger() || leftType->isEnum())) {
            resultType = leftType->isEnum() ? makeInt() : leftType;
        } else if (rightType && (rightType->isInteger() || rightType->isEnum())) {
            resultType = rightType->isEnum() ? makeInt() : rightType;
        } else {
            resultType = makeInt();  // 默认 int
        }
    }
    
    if (!resultType) return Operand::none();
    
    Operand result = Operand::temp(newTemp(), resultType);
    emit(op, result, left, right);
    return result;
}

Operand IRGenerator::genAssignment(ast::BinaryExpr* expr) {
    Operand addr = genLValueAddr(expr->left.get());
    Operand value = genExpr(expr->right.get());
    emit(IROpcode::Store, addr, value);
    return value;
}

Operand IRGenerator::genLogicalAnd(ast::BinaryExpr* expr) {
    std::string falseLabel = newLabel("and_false");
    std::string endLabel = newLabel("and_end");
    
    Operand result = Operand::temp(newTemp(), makeInt());
    
    Operand left = genExpr(expr->left.get());
    emitJumpFalse(left, falseLabel);
    
    Operand right = genExpr(expr->right.get());
    emitJumpFalse(right, falseLabel);
    
    emit(IROpcode::Assign, result, Operand::intConst(1));
    emitJump(endLabel);
    
    emitLabel(falseLabel);
    emit(IROpcode::Assign, result, Operand::intConst(0));
    
    emitLabel(endLabel);
    return result;
}

Operand IRGenerator::genLogicalOr(ast::BinaryExpr* expr) {
    std::string trueLabel = newLabel("or_true");
    std::string endLabel = newLabel("or_end");
    
    Operand result = Operand::temp(newTemp(), makeInt());
    
    Operand left = genExpr(expr->left.get());
    emitJumpTrue(left, trueLabel);
    
    Operand right = genExpr(expr->right.get());
    emitJumpTrue(right, trueLabel);
    
    emit(IROpcode::Assign, result, Operand::intConst(0));
    emitJump(endLabel);
    
    emitLabel(trueLabel);
    emit(IROpcode::Assign, result, Operand::intConst(1));
    
    emitLabel(endLabel);
    return result;
}

Operand IRGenerator::genUnaryExpr(ast::UnaryExpr* expr) {
    if (!expr) return Operand::none();
    
    using Op = ast::UnaryOp;
    
    switch (expr->op) {
        case Op::Plus:
            return genExpr(expr->operand.get());
        
        case Op::Minus: {
            Operand operand = genExpr(expr->operand.get());
            TypePtr resultType = getExprSemType(expr);
            if (!resultType) return Operand::none();
            Operand result = Operand::temp(newTemp(), resultType);
            emit(IROpcode::Neg, result, operand);
            return result;
        }
        
        case Op::Not: {
            Operand operand = genExpr(expr->operand.get());
            Operand result = Operand::temp(newTemp(), makeInt());
            emit(IROpcode::LogicalNot, result, operand);
            return result;
        }
        
        case Op::BitNot: {
            Operand operand = genExpr(expr->operand.get());
            TypePtr resultType = getExprSemType(expr);
            if (!resultType) return Operand::none();
            Operand result = Operand::temp(newTemp(), resultType);
            emit(IROpcode::BitNot, result, operand);
            return result;
        }
        
        case Op::PreInc:
        case Op::PreDec: {
            Operand addr = genLValueAddr(expr->operand.get());
            TypePtr operandType = getExprSemType(expr->operand.get());
            if (!operandType) return Operand::none();
            Operand val = Operand::temp(newTemp(), operandType);
            emit(IROpcode::Load, val, addr);

            // 对于指针，增量应该是元素大小
            int64_t increment = 1;
            if (operandType->isPointer()) {
                auto ptrType = static_cast<PointerType*>(operandType.get());
                if (ptrType->pointee) {
                    increment = ptrType->pointee->size();
                }
            }
            
            Operand delta = Operand::intConst(increment);
            Operand newVal = Operand::temp(newTemp(), val.type);
            emit(expr->op == Op::PreInc ? IROpcode::Add : IROpcode::Sub, newVal, val, delta);
            emit(IROpcode::Store, addr, newVal);
            return newVal;
        }
        
        case Op::PostInc:
        case Op::PostDec: {
            Operand addr = genLValueAddr(expr->operand.get());
            TypePtr operandType = getExprSemType(expr->operand.get());
            if (!operandType) return Operand::none();
            Operand oldVal = Operand::temp(newTemp(), operandType);
            emit(IROpcode::Load, oldVal, addr);

            // 对于指针，增量应该是元素大小
            int64_t increment = 1;
            if (operandType->isPointer()) {
                auto ptrType = static_cast<PointerType*>(operandType.get());
                if (ptrType->pointee) {
                    increment = ptrType->pointee->size();
                }
            }
            
            Operand delta = Operand::intConst(increment);
            Operand newVal = Operand::temp(newTemp(), oldVal.type);
            emit(expr->op == Op::PostInc ? IROpcode::Add : IROpcode::Sub, newVal, oldVal, delta);
            emit(IROpcode::Store, addr, newVal);
            return oldVal;
        }
        
        case Op::Deref: {
            Operand ptr = genExpr(expr->operand.get());
            TypePtr resultType = getExprSemType(expr);
            if (!resultType) return Operand::none();
            Operand result = Operand::temp(newTemp(), resultType);
            emit(IROpcode::Load, result, ptr);
            return result;
        }
        
        case Op::AddrOf:
            return genLValueAddr(expr->operand.get());
        
        case Op::Sizeof:
            return genSizeofExpr(expr);
        
        default:
            return Operand::none();
    }
}

Operand IRGenerator::genSizeofExpr(ast::UnaryExpr* expr) {
    // sizeof 是编译时常量

    // 特殊处理字符串字面量：sizeof("str") = 字符串长度 + 1（包括终止符）
    if (auto strLit = dynamic_cast<ast::StringLiteral*>(expr->operand.get())) {
        int size = static_cast<int>(strLit->value.size()) + 1;
        return Operand::intConst(size, makeLong(true));
    }

    TypePtr type = getExprSemType(expr->operand.get());
    int size = type ? type->size() : 0;
    return Operand::intConst(size, makeLong(true));
}

Operand IRGenerator::genCallExpr(ast::CallExpr* expr) {
    if (!expr) return Operand::none();
    
    std::vector<Operand> args;
    for (auto& arg : expr->arguments) {
        args.push_back(genExpr(arg.get()));
    }
    
    // 逆序传参（符合 cdecl 调用约定）
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        emit(IROpcode::Param, Operand::none(), *it);
    }
    
    // 处理函数调用：区分直接调用和函数指针调用
    Operand callee;
    if (auto identExpr = dynamic_cast<ast::IdentExpr*>(expr->callee.get())) {
        // 直接函数调用：使用 Label 操作数
        auto sym = symTable_.lookup(identExpr->name);
        if (sym && sym->kind == SymbolKind::Function) {
            callee = Operand::label(identExpr->name);
        } else {
            // 函数指针变量
            callee = genExpr(expr->callee.get());
        }
    } else {
        // 函数指针表达式
        callee = genExpr(expr->callee.get());
    }
    
    TypePtr retType = getExprSemType(expr);
    Operand result = Operand::none();
    if (retType && !retType->isVoid()) {
        result = Operand::temp(newTemp(), retType);
    }
    
    emit(IROpcode::Call, result, callee, Operand::intConst(args.size()));
    return result;
}

Operand IRGenerator::genSubscriptExpr(ast::SubscriptExpr* expr) {
    if (!expr) return Operand::none();
    
    Operand base = genExpr(expr->array.get());
    Operand index = genExpr(expr->index.get());
    
    TypePtr elemType = getExprSemType(expr);
    if (!elemType) return Operand::none();
    
    Operand addr = Operand::temp(newTemp(), makePointer(elemType));
    emit(IROpcode::IndexAddr, addr, base, index);
    
    Operand result = Operand::temp(newTemp(), elemType);
    emit(IROpcode::Load, result, addr);
    return result;
}

Operand IRGenerator::genMemberExpr(ast::MemberExpr* expr) {
    if (!expr) return Operand::none();
    
    Operand base;
    if (expr->isArrow) {
        base = genExpr(expr->object.get());
    } else {
        base = genLValueAddr(expr->object.get());
    }
    
    // 从符号表获取成员偏移
    TypePtr baseType = getExprSemType(expr->object.get());
    if (!baseType) return Operand::none();
    
    int offset = 0;
    TypePtr memberType = nullptr;
    
    if (baseType->isPointer() && expr->isArrow) {
        auto ptrType = static_cast<PointerType*>(baseType.get());
        baseType = ptrType->pointee;
    }
    
    if (baseType->isStruct()) {
        auto structType = static_cast<StructType*>(baseType.get());
        const Member* member = structType->findMember(expr->member);
        if (member) {
            offset = member->offset;
            memberType = member->type;
        }
    } else if (baseType->isUnion()) {
        auto unionType = static_cast<UnionType*>(baseType.get());
        const Member* member = unionType->findMember(expr->member);
        if (member) {
            offset = member->offset;
            memberType = member->type;
        }
    }
    
    if (!memberType) return Operand::none();
    
    Operand addr = Operand::temp(newTemp(), makePointer(memberType));
    emit(IROpcode::MemberAddr, addr, base, Operand::intConst(offset));
    
    Operand result = Operand::temp(newTemp(), memberType);
    emit(IROpcode::Load, result, addr);
    return result;
}

Operand IRGenerator::genCastExpr(ast::CastExpr* expr) {
    if (!expr) return Operand::none();
    
    Operand src = genExpr(expr->operand.get());
    TypePtr targetType = getExprSemType(expr);
    
    // 数组转换为指针时，需要获取数组地址（数组衰减）
    if (src.type && src.type->isArray() && targetType && targetType->isPointer()) {
        auto arrType = static_cast<ArrayType*>(src.type.get());
        Operand addr = Operand::temp(newTemp(), makePointer(arrType->elementType));
        emit(IROpcode::LoadAddr, addr, src);
        src = addr;
    }
    
    return convertType(src, targetType);
}

Operand IRGenerator::genConditionalExpr(ast::ConditionalExpr* expr) {
    if (!expr) return Operand::none();

    std::string falseLabel = newLabel("cond_false");
    std::string endLabel = newLabel("cond_end");

    // 先计算条件
    Operand cond = genExpr(expr->condition.get());
    emitJumpFalse(cond, falseLabel);

    // 计算 true 分支
    Operand trueVal = genExpr(expr->thenExpr.get());
    
    // 推断结果类型
    TypePtr resultType = getExprSemType(expr);
    if (!resultType) {
        resultType = trueVal.type;
    }
    if (!resultType) {
        resultType = getExprSemType(expr->thenExpr.get());
    }
    if (!resultType) {
        resultType = makeInt();  // 默认类型
    }
    
    Operand result = Operand::temp(newTemp(), resultType);
    emit(IROpcode::Assign, result, trueVal);
    emitJump(endLabel);

    emitLabel(falseLabel);
    Operand falseVal = genExpr(expr->elseExpr.get());
    emit(IROpcode::Assign, result, falseVal);

    emitLabel(endLabel);
    return result;
}

//=============================================================================
// 左值地址生成
//=============================================================================

Operand IRGenerator::genLValueAddr(ast::Expr* expr) {
    if (!expr) return Operand::none();

    if (auto identExpr = dynamic_cast<ast::IdentExpr*>(expr)) {
        auto sym = symTable_.lookup(identExpr->name);
        if (!sym) return Operand::none();

        Operand addr = Operand::temp(newTemp(), makePointer(sym->type));
        if (symTable_.isGlobalScope() || sym->storage == StorageClass::Static) {
            emit(IROpcode::LoadAddr, addr, Operand::global(identExpr->name, sym->type));
        } else {
            // 使用唯一的 IR 名称
            auto it = varIRNames_.find(sym.get());
            std::string irName = (it != varIRNames_.end()) ? it->second : identExpr->name;
            emit(IROpcode::LoadAddr, addr, Operand::variable(irName, sym->type));
        }
        return addr;
    }
    
    if (auto unaryExpr = dynamic_cast<ast::UnaryExpr*>(expr)) {
        if (unaryExpr->op == ast::UnaryOp::Deref) {
            return genExpr(unaryExpr->operand.get());
        }
    }
    
    if (auto subscriptExpr = dynamic_cast<ast::SubscriptExpr*>(expr)) {
        Operand base = genExpr(subscriptExpr->array.get());
        Operand index = genExpr(subscriptExpr->index.get());
        
        TypePtr elemType = getExprSemType(expr);
        if (!elemType) return Operand::none();
        Operand addr = Operand::temp(newTemp(), makePointer(elemType));
        emit(IROpcode::IndexAddr, addr, base, index);
        return addr;
    }
    
    if (auto memberExpr = dynamic_cast<ast::MemberExpr*>(expr)) {
        Operand base;
        if (memberExpr->isArrow) {
            base = genExpr(memberExpr->object.get());
        } else {
            base = genLValueAddr(memberExpr->object.get());
        }
        
        // 从类型系统获取成员偏移
        TypePtr baseType = getExprSemType(memberExpr->object.get());
        if (!baseType) return Operand::none();
        
        int offset = 0;
        TypePtr memberType = nullptr;
        
        if (baseType->isPointer() && memberExpr->isArrow) {
            auto ptrType = static_cast<PointerType*>(baseType.get());
            baseType = ptrType->pointee;
        }
        
        if (baseType->isStruct()) {
            auto structType = static_cast<StructType*>(baseType.get());
            const Member* member = structType->findMember(memberExpr->member);
            if (member) {
                offset = member->offset;
                memberType = member->type;
            }
        } else if (baseType->isUnion()) {
            auto unionType = static_cast<UnionType*>(baseType.get());
            const Member* member = unionType->findMember(memberExpr->member);
            if (member) {
                offset = member->offset;
                memberType = member->type;
            }
        }
        
        if (!memberType) return Operand::none();
        
        Operand addr = Operand::temp(newTemp(), makePointer(memberType));
        emit(IROpcode::MemberAddr, addr, base, Operand::intConst(offset));
        return addr;
    }
    
    return Operand::none();
}

//=============================================================================
// 辅助函数：类型转换
//=============================================================================

TypePtr IRGenerator::getExprSemType(ast::Expr* expr) {
    if (!expr) return nullptr;
    
    // 对于标识符表达式，优先从符号表获取类型（更准确）
    if (auto identExpr = dynamic_cast<ast::IdentExpr*>(expr)) {
        auto sym = symTable_.lookup(identExpr->name);
        if (sym) return sym->type;
    }
    
    // 对于成员表达式，从结构体/联合体类型中查找成员类型
    if (auto memberExpr = dynamic_cast<ast::MemberExpr*>(expr)) {
        TypePtr baseType = getExprSemType(memberExpr->object.get());
        if (!baseType) return nullptr;
        
        // 如果是箭头访问，先解引用指针
        if (memberExpr->isArrow && baseType->isPointer()) {
            auto ptrType = static_cast<PointerType*>(baseType.get());
            baseType = ptrType->pointee;
        }
        
        if (baseType->isStruct()) {
            auto structType = static_cast<StructType*>(baseType.get());
            const Member* member = structType->findMember(memberExpr->member);
            if (member) return member->type;
        } else if (baseType->isUnion()) {
            auto unionType = static_cast<UnionType*>(baseType.get());
            const Member* member = unionType->findMember(memberExpr->member);
            if (member) return member->type;
        }
        return nullptr;
    }
    
    // 对于解引用表达式，获取指针指向的类型
    if (auto unaryExpr = dynamic_cast<ast::UnaryExpr*>(expr)) {
        if (unaryExpr->op == ast::UnaryOp::Deref) {
            TypePtr operandType = getExprSemType(unaryExpr->operand.get());
            if (operandType && operandType->isPointer()) {
                auto ptrType = static_cast<PointerType*>(operandType.get());
                return ptrType->pointee;
            }
        }
        // 对于其他一元运算符，检查 exprType
    }
    
    // 对于函数调用表达式，从函数签名获取返回类型
    if (auto callExpr = dynamic_cast<ast::CallExpr*>(expr)) {
        // 先检查 callee 是否是函数名
        if (auto identExpr = dynamic_cast<ast::IdentExpr*>(callExpr->callee.get())) {
            auto sym = symTable_.lookup(identExpr->name);
            if (sym && sym->type && sym->type->isFunction()) {
                auto funcType = static_cast<FunctionType*>(sym->type.get());
                return funcType->returnType;
            }
        }
        // 如果是函数指针调用，获取指针指向的函数类型
        TypePtr calleeType = getExprSemType(callExpr->callee.get());
        if (calleeType) {
            if (calleeType->isPointer()) {
                auto ptrType = static_cast<PointerType*>(calleeType.get());
                if (ptrType->pointee && ptrType->pointee->isFunction()) {
                    auto funcType = static_cast<FunctionType*>(ptrType->pointee.get());
                    return funcType->returnType;
                }
            } else if (calleeType->isFunction()) {
                auto funcType = static_cast<FunctionType*>(calleeType.get());
                return funcType->returnType;
            }
        }
    }
    
    if (!expr->exprType) return nullptr;
    
    // 否则从AST节点的exprType转换
    return astTypeToSemType(expr->exprType.get());
}

TypePtr IRGenerator::astTypeToSemType(ast::Type* astType) {
    if (!astType) return nullptr;
    
    if (auto basicType = dynamic_cast<ast::BasicType*>(astType)) {
        switch (basicType->kind) {
            case ast::BasicTypeKind::Void: return makeVoid();
            case ast::BasicTypeKind::Char: return makeChar();
            case ast::BasicTypeKind::Short: return makeShort();
            case ast::BasicTypeKind::Int: return makeInt();
            case ast::BasicTypeKind::Long: return makeLong();
            case ast::BasicTypeKind::LongLong: return makeLongLong();
            case ast::BasicTypeKind::Float: return makeFloat();
            case ast::BasicTypeKind::Double: return makeDouble();
            case ast::BasicTypeKind::LongDouble: 
                return std::make_shared<FloatType>(FloatKind::LongDouble);
            case ast::BasicTypeKind::UChar: return makeChar(true);
            case ast::BasicTypeKind::UShort: return makeShort(true);
            case ast::BasicTypeKind::UInt: return makeInt(true);
            case ast::BasicTypeKind::ULong: return makeLong(true);
            case ast::BasicTypeKind::ULongLong: return makeLongLong(true);
            case ast::BasicTypeKind::SChar: return makeChar(false);
        }
    } else if (auto ptrType = dynamic_cast<ast::PointerType*>(astType)) {
        TypePtr pointee = astTypeToSemType(ptrType->pointee.get());
        return pointee ? makePointer(pointee) : makePointer(makeVoid());
    } else if (auto arrType = dynamic_cast<ast::ArrayType*>(astType)) {
        TypePtr elemType = astTypeToSemType(arrType->elementType.get());
        int64_t size = -1;
        if (arrType->size) {
            // 尝试从各种常量表达式获取数组大小
            if (auto intLit = dynamic_cast<ast::IntLiteral*>(arrType->size.get())) {
                size = intLit->value;
            }
        }
        return elemType ? makeArray(elemType, size) : nullptr;
    } else if (auto funcType = dynamic_cast<ast::FunctionType*>(astType)) {
        TypePtr retType = astTypeToSemType(funcType->returnType.get());
        auto ft = std::make_shared<FunctionType>(retType ? retType : makeInt());
        ft->isVariadic = funcType->isVariadic;
        for (const auto& pt : funcType->paramTypes) {
            TypePtr paramType = astTypeToSemType(pt.get());
            ft->paramTypes.push_back(paramType ? paramType : makeInt());
        }
        return ft;
    } else if (auto recordType = dynamic_cast<ast::RecordType*>(astType)) {
        // 从符号表查找已定义的类型
        auto tag = symTable_.lookupTag(recordType->name);
        if (tag) return tag->type;
        // 返回不完整类型
        if (recordType->isUnion) {
            return std::make_shared<UnionType>(recordType->name);
        } else {
            return std::make_shared<StructType>(recordType->name);
        }
    } else if (auto enumType = dynamic_cast<ast::EnumType*>(astType)) {
        auto tag = symTable_.lookupTag(enumType->name);
        if (tag) return tag->type;
        return makeInt();  // 枚举默认为 int
    } else if (auto typedefType = dynamic_cast<ast::TypedefType*>(astType)) {
        // typedef类型：从符号表查找
        auto sym = symTable_.lookup(typedefType->name);
        if (sym && sym->kind == SymbolKind::TypeDef) {
            return sym->type;
        }
        return nullptr;
    }
    
    return nullptr;
}

Operand IRGenerator::convertType(Operand src, TypePtr targetType) {
    if (!targetType || !src.type) return src;
    
    if (areTypesCompatible(src.type, targetType)) {
        return src;
    }
    
    Operand result = Operand::temp(newTemp(), targetType);
    
    if (src.type->isInteger() && targetType->isFloat()) {
        emit(IROpcode::IntToFloat, result, src);
        return result;
    }
    
    if (src.type->isFloat() && targetType->isInteger()) {
        emit(IROpcode::FloatToInt, result, src);
        return result;
    }
    
    if (src.type->isInteger() && targetType->isInteger()) {
        if (src.type->size() < targetType->size()) {
            emit(IROpcode::IntExtend, result, src);
        } else if (src.type->size() > targetType->size()) {
            emit(IROpcode::IntTrunc, result, src);
        } else {
            emit(IROpcode::Assign, result, src);
        }
        return result;
    }
    
    if ((src.type->isPointer() && targetType->isInteger()) ||
        (src.type->isInteger() && targetType->isPointer())) {
        emit(src.type->isPointer() ? IROpcode::PtrToInt : IROpcode::IntToPtr, result, src);
        return result;
    }
    
    emit(IROpcode::Assign, result, src);
    return result;
}

} // namespace semantic
} // namespace cdd
