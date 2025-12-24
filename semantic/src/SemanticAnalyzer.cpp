/**
 * @file SemanticAnalyzer.cpp
 * @brief 语义分析器实现
 * @author mozihe
 */

#include "SemanticAnalyzer.h"

namespace cdd {
namespace semantic {

SemanticAnalyzer::SemanticAnalyzer() = default;
SemanticAnalyzer::~SemanticAnalyzer() = default;

void SemanticAnalyzer::error(const SourceLocation& loc, const std::string& msg) {
    errors_.push_back({loc, msg, false});
}

void SemanticAnalyzer::warning(const SourceLocation& loc, const std::string& msg) {
    warnings_.push_back({loc, msg, true});
}

/**
 * @brief 分析翻译单元
 * 
 * 遍历所有顶层声明进行语义分析。
 * 返回 true 表示没有错误。
 */
bool SemanticAnalyzer::analyze(ast::TranslationUnit* unit) {
    if (!unit) return false;
    
    for (auto& decl : unit->declarations) {
        analyzeDecl(decl.get());
    }
    
    return errors_.empty();
}

void SemanticAnalyzer::analyzeDeclaration(ast::Decl* decl) {
    analyzeDecl(decl);
}

//=============================================================================
// 声明处理
//=============================================================================

void SemanticAnalyzer::analyzeDecl(ast::Decl* decl) {
    if (!decl) return;
    
    if (auto varDecl = dynamic_cast<ast::VarDecl*>(decl)) {
        analyzeVarDecl(varDecl);
    } else if (auto funcDecl = dynamic_cast<ast::FunctionDecl*>(decl)) {
        analyzeFunctionDecl(funcDecl);
    } else if (auto recordDecl = dynamic_cast<ast::RecordDecl*>(decl)) {
        analyzeRecordDecl(recordDecl);
    } else if (auto enumDecl = dynamic_cast<ast::EnumDecl*>(decl)) {
        analyzeEnumDecl(enumDecl);
    } else if (auto typedefDecl = dynamic_cast<ast::TypedefDecl*>(decl)) {
        analyzeTypedefDecl(typedefDecl);
    }
}

/**
 * @brief 分析变量声明
 * 
 * 处理流程：
 * 1. 解析变量类型
 * 2. 检查类型有效性（不能为 void）
 * 3. 检查重复定义（处理 extern 声明）
 * 4. 添加符号到符号表
 * 5. 分析初始化表达式（类型兼容性检查）
 */
void SemanticAnalyzer::analyzeVarDecl(ast::VarDecl* decl) {
    if (!decl) return;
    
    // 1. 解析类型
    TypePtr varType = resolveAstType(decl->type.get());
    if (!varType) {
        error(decl->location, "变量声明中存在未知类型");
        return;
    }
    
    // 2. 检查 void 类型变量
    if (varType->isVoid()) {
        error(decl->location, "不能声明 void 类型的变量");
        return;
    }
    
    std::string name = decl->name;
    if (name.empty()) {
        error(decl->location, "变量声明缺少名称");
        return;
    }
    
    // 3. 检查重复定义
    auto existingSym = symTable_.lookupLocal(name);
    if (existingSym) {
        // 允许 extern 声明后跟定义，或多个 extern 声明
        bool isExternDecl = (decl->storage == ast::StorageClass::Extern);
        bool existingIsExtern = (existingSym->storage == StorageClass::Extern);
        
        if (isExternDecl && existingIsExtern) {
            // 多个 extern 声明是允许的，检查类型一致性
            if (!areTypesCompatible(existingSym->type, varType)) {
                error(decl->location, "'" + name + "' 的类型声明冲突");
            }
            return;  // 不需要重新添加符号
        } else if (isExternDecl && !existingIsExtern) {
            // extern 声明在定义之后，允许
            return;
        } else if (!isExternDecl && existingIsExtern) {
            // 定义跟在 extern 声明之后，更新符号
            existingSym->storage = StorageClass::None;
            // 继续分析初始化表达式
        } else {
            error(decl->location, "'" + name + "' 重复定义");
            return;
        }
    }
    
    // 创建符号（只有当不存在时）
    if (!existingSym) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Variable, varType, decl->location);
        
        // 处理存储类
        switch (decl->storage) {
            case ast::StorageClass::Static: sym->storage = StorageClass::Static; break;
            case ast::StorageClass::Extern: sym->storage = StorageClass::Extern; break;
            case ast::StorageClass::Register: sym->storage = StorageClass::Register; break;
            case ast::StorageClass::Auto: sym->storage = StorageClass::Auto; break;
            default: sym->storage = StorageClass::None; break;
        }
        
        symTable_.addSymbol(sym);
    }
    
    // 分析初始化表达式
    if (decl->initializer) {
        TypePtr initType = analyzeExpr(decl->initializer.get());
        if (initType && !canImplicitlyConvert(initType, varType)) {
            error(decl->location, "初始化表达式类型不兼容");
        }
    }
}

/**
 * @brief 分析函数声明/定义
 * 
 * 处理流程：
 * 1. 解析返回类型和参数类型
 * 2. 创建函数类型（处理数组参数衰减）
 * 3. 检查重复声明（类型兼容性）
 * 4. 添加函数符号到符号表
 * 5. 如果有函数体：
 *    - 创建函数作用域
 *    - 添加参数符号
 *    - 分析函数体
 */
void SemanticAnalyzer::analyzeFunctionDecl(ast::FunctionDecl* decl) {
    if (!decl) return;
    
    // 1. 解析返回类型
    TypePtr returnType = resolveAstType(decl->returnType.get());
    if (!returnType) {
        returnType = makeInt();
    }
    
    // 2. 创建函数类型
    auto funcType = std::make_shared<FunctionType>(returnType);
    funcType->isVariadic = decl->isVariadic;
    
    // 处理参数（数组参数衰减为指针）
    for (const auto& param : decl->params) {
        TypePtr paramType = resolveAstType(param->type.get());
        if (!paramType) paramType = makeInt();
        
        if (paramType->isArray()) {
            auto arr = static_cast<ArrayType*>(paramType.get());
            paramType = makePointer(arr->elementType->clone());
        }
        
        funcType->paramTypes.push_back(paramType);
        funcType->paramNames.push_back(param->name);
    }
    
    // 3. 检查重复声明
    std::string funcName = decl->name;
    auto existingSym = symTable_.lookup(funcName);
    if (existingSym) {
        if (!existingSym->type->isFunction()) {
            error(decl->location, "'" + funcName + "' 被重新声明为不同类型的符号");
            return;
        }
        if (!areTypesCompatible(existingSym->type, funcType)) {
            error(decl->location, "'" + funcName + "' 的类型声明冲突");
            return;
        }
    } else {
        auto sym = std::make_shared<Symbol>(funcName, SymbolKind::Function, funcType, decl->location);
        sym->globalLabel = funcName;
        symTable_.addSymbol(sym);
    }
    
    // 如果有函数体
    if (decl->body) {
        int funcScopeId = symTable_.enterScope(ScopeKind::Function);
        decl->scopeId = funcScopeId;
        symTable_.setCurrentFunctionInfo(funcName, returnType);
        
        // 添加参数到作用域
        for (size_t i = 0; i < decl->params.size(); ++i) {
            const auto& param = decl->params[i];
            if (!param->name.empty()) {
                auto paramSym = std::make_shared<Symbol>(
                    param->name, 
                    SymbolKind::Parameter, 
                    funcType->paramTypes[i], 
                    param->location
                );
                symTable_.addSymbol(paramSym);
            }
        }
        
        analyzeCompoundStmt(decl->body.get());
        symTable_.exitScope();
    }
}

void SemanticAnalyzer::analyzeRecordDecl(ast::RecordDecl* decl) {
    if (!decl) return;
    
    std::string tagName = decl->name;
    
    TypePtr recordType;
    if (decl->isUnion) {
        auto ut = std::make_shared<UnionType>(tagName);
        for (const auto& field : decl->fields) {
            TypePtr fieldType = resolveAstType(field->type.get());
            if (!fieldType) {
                error(field->location, "无效的成员类型");
                continue;
            }
            ut->members.push_back({field->name, fieldType, 0});
        }
        ut->isComplete = !decl->fields.empty();
        recordType = ut;
    } else {
        auto st = std::make_shared<StructType>(tagName);
        int offset = 0;
        
        for (const auto& field : decl->fields) {
            TypePtr fieldType = resolveAstType(field->type.get());
            if (!fieldType) {
                error(field->location, "无效的成员类型");
                continue;
            }
            
            // 处理匿名结构体/联合体：将其成员提升到父结构体
            if (field->name.empty() && (fieldType->isStruct() || fieldType->isUnion())) {
                if (fieldType->isStruct()) {
                    auto anon = static_cast<StructType*>(fieldType.get());
                    for (const auto& member : anon->members) {
                        if (st->findMember(member.name)) {
                            error(field->location, "成员 '" + member.name + "' 重复定义");
                            continue;
                        }
                        int align = member.type->alignment();
                        offset = (offset + align - 1) / align * align;
                        st->members.push_back({member.name, member.type->clone(), offset});
                        offset += member.type->size();
                    }
                } else {
                    auto anon = static_cast<UnionType*>(fieldType.get());
                    for (const auto& member : anon->members) {
                        if (st->findMember(member.name)) {
                            error(field->location, "成员 '" + member.name + "' 重复定义");
                            continue;
                        }
                        st->members.push_back({member.name, member.type->clone(), offset});
                    }
                    // 联合体只占用最大成员的大小
                    int maxSize = 0;
                    for (const auto& member : anon->members) {
                        maxSize = std::max(maxSize, member.type->size());
                    }
                    offset += maxSize;
                }
            } else {
                if (st->findMember(field->name)) {
                    error(field->location, "成员 '" + field->name + "' 重复定义");
                    continue;
                }
                
                int align = fieldType->alignment();
                offset = (offset + align - 1) / align * align;
                st->members.push_back({field->name, fieldType, offset});
                offset += fieldType->size();
            }
        }
        
        st->isComplete = !decl->fields.empty();
        recordType = st;
    }
    
    if (!tagName.empty()) {
        auto sym = std::make_shared<Symbol>(
            tagName, 
            decl->isUnion ? SymbolKind::UnionTag : SymbolKind::StructTag, 
            recordType, 
            decl->location
        );
        symTable_.addTag(sym);
    }
}

void SemanticAnalyzer::analyzeEnumDecl(ast::EnumDecl* decl) {
    if (!decl) return;
    
    std::string tagName = decl->name;
    auto enumType = std::make_shared<EnumType>(tagName);
    
    // 先注册枚举标签（允许常量表达式引用同枚举中已定义的常量）
    if (!tagName.empty()) {
        auto tagSym = std::make_shared<Symbol>(tagName, SymbolKind::EnumTag, enumType, decl->location);
        symTable_.addTag(tagSym);
    }
    
    int64_t nextValue = 0;
    for (const auto& constant : decl->constants) {
        std::string name = constant->name;
        
        if (constant->value) {
            TypePtr valType = analyzeExpr(constant->value.get());
            if (!valType || !valType->isInteger()) {
                error(decl->location, "枚举值必须是整数");
            } else {
                int64_t constVal;
                if (evaluateConstantExpr(constant->value.get(), constVal)) {
                    nextValue = constVal;
                }
            }
        }
        
        enumType->enumerators[name] = nextValue;
        
        auto sym = std::make_shared<Symbol>(name, SymbolKind::EnumConstant, makeInt(), constant->location);
        if (!symTable_.addSymbol(sym)) {
            error(constant->location, "枚举常量 '" + name + "' 重复定义");
        }
        
        ++nextValue;
    }
}

void SemanticAnalyzer::analyzeTypedefDecl(ast::TypedefDecl* decl) {
    if (!decl) return;
    
    TypePtr type = resolveAstType(decl->underlyingType.get());
    if (!type) {
        error(decl->location, "无效的 typedef 类型");
        return;
    }
    
    std::string name = decl->name;
    auto sym = std::make_shared<Symbol>(name, SymbolKind::TypeDef, type, decl->location);
    if (!symTable_.addSymbol(sym)) {
        error(decl->location, "typedef '" + name + "' 重复定义");
    }
}

//=============================================================================
// 语句处理
//=============================================================================

void SemanticAnalyzer::analyzeStmt(ast::Stmt* stmt) {
    if (!stmt) return;
    
    if (auto exprStmt = dynamic_cast<ast::ExprStmt*>(stmt)) {
        if (exprStmt->expr) analyzeExpr(exprStmt->expr.get());
    } else if (auto compoundStmt = dynamic_cast<ast::CompoundStmt*>(stmt)) {
        analyzeCompoundStmt(compoundStmt);
    } else if (auto ifStmt = dynamic_cast<ast::IfStmt*>(stmt)) {
        analyzeIfStmt(ifStmt);
    } else if (auto whileStmt = dynamic_cast<ast::WhileStmt*>(stmt)) {
        analyzeWhileStmt(whileStmt);
    } else if (auto doWhileStmt = dynamic_cast<ast::DoWhileStmt*>(stmt)) {
        analyzeDoWhileStmt(doWhileStmt);
    } else if (auto forStmt = dynamic_cast<ast::ForStmt*>(stmt)) {
        analyzeForStmt(forStmt);
    } else if (auto switchStmt = dynamic_cast<ast::SwitchStmt*>(stmt)) {
        analyzeSwitchStmt(switchStmt);
    } else if (auto caseStmt = dynamic_cast<ast::CaseStmt*>(stmt)) {
        analyzeCaseStmt(caseStmt);
    } else if (auto defaultStmt = dynamic_cast<ast::DefaultStmt*>(stmt)) {
        analyzeDefaultStmt(defaultStmt);
    } else if (auto breakStmt = dynamic_cast<ast::BreakStmt*>(stmt)) {
        analyzeBreakStmt(breakStmt);
    } else if (auto continueStmt = dynamic_cast<ast::ContinueStmt*>(stmt)) {
        analyzeContinueStmt(continueStmt);
    } else if (auto returnStmt = dynamic_cast<ast::ReturnStmt*>(stmt)) {
        analyzeReturnStmt(returnStmt);
    } else if (auto labelStmt = dynamic_cast<ast::LabelStmt*>(stmt)) {
        analyzeStmt(labelStmt->stmt.get());
    }
}

void SemanticAnalyzer::analyzeCompoundStmt(ast::CompoundStmt* stmt) {
    if (!stmt) return;
    
    int scopeId = symTable_.enterScope(ScopeKind::Block);
    stmt->scopeId = scopeId;
    
    for (auto& item : stmt->items) {
        if (std::holds_alternative<ast::DeclPtr>(item)) {
            analyzeDecl(std::get<ast::DeclPtr>(item).get());
        } else if (std::holds_alternative<ast::StmtPtr>(item)) {
            analyzeStmt(std::get<ast::StmtPtr>(item).get());
        }
    }
    
    symTable_.exitScope();
}

void SemanticAnalyzer::analyzeIfStmt(ast::IfStmt* stmt) {
    if (!stmt) return;
    
    TypePtr condType = analyzeExpr(stmt->condition.get());
    checkCondition(condType, stmt->location);
    
    analyzeStmt(stmt->thenStmt.get());
    if (stmt->elseStmt) analyzeStmt(stmt->elseStmt.get());
}

void SemanticAnalyzer::analyzeWhileStmt(ast::WhileStmt* stmt) {
    if (!stmt) return;
    
    TypePtr condType = analyzeExpr(stmt->condition.get());
    checkCondition(condType, stmt->location);
    
    ++loopDepth_;
    analyzeStmt(stmt->body.get());
    --loopDepth_;
}

void SemanticAnalyzer::analyzeDoWhileStmt(ast::DoWhileStmt* stmt) {
    if (!stmt) return;
    
    ++loopDepth_;
    analyzeStmt(stmt->body.get());
    --loopDepth_;
    
    TypePtr condType = analyzeExpr(stmt->condition.get());
    checkCondition(condType, stmt->location);
}

void SemanticAnalyzer::analyzeForStmt(ast::ForStmt* stmt) {
    if (!stmt) return;
    
    int scopeId = symTable_.enterScope(ScopeKind::Block);
    stmt->scopeId = scopeId;
    
    if (std::holds_alternative<ast::DeclList>(stmt->init)) {
        for (auto& decl : std::get<ast::DeclList>(stmt->init)) {
            analyzeDecl(decl.get());
        }
    } else if (std::holds_alternative<ast::StmtPtr>(stmt->init)) {
        analyzeStmt(std::get<ast::StmtPtr>(stmt->init).get());
    }
    
    if (stmt->condition) {
        TypePtr condType = analyzeExpr(stmt->condition.get());
        checkCondition(condType, stmt->location);
    }
    
    if (stmt->increment) analyzeExpr(stmt->increment.get());
    
    ++loopDepth_;
    analyzeStmt(stmt->body.get());
    --loopDepth_;
    
    symTable_.exitScope();
}

void SemanticAnalyzer::analyzeSwitchStmt(ast::SwitchStmt* stmt) {
    if (!stmt) return;
    
    TypePtr condType = analyzeExpr(stmt->condition.get());
    if (!condType || !condType->isInteger()) {
        error(stmt->location, "switch 表达式必须是整数类型");
    }
    
    ++switchDepth_;
    ++loopDepth_;
    analyzeStmt(stmt->body.get());
    --loopDepth_;
    --switchDepth_;
}

void SemanticAnalyzer::analyzeCaseStmt(ast::CaseStmt* stmt) {
    if (!stmt) return;
    
    if (switchDepth_ == 0) {
        error(stmt->location, "'case' 语句不在 switch 语句中");
        return;
    }
    
    TypePtr valType = analyzeExpr(stmt->value.get());
    if (!valType || !valType->isInteger()) {
        error(stmt->location, "case 值必须是整数常量");
    }
    
    if (stmt->stmt) analyzeStmt(stmt->stmt.get());
}

void SemanticAnalyzer::analyzeDefaultStmt(ast::DefaultStmt* stmt) {
    if (!stmt) return;
    
    if (switchDepth_ == 0) {
        error(stmt->location, "'default' 语句不在 switch 语句中");
        return;
    }
    
    if (stmt->stmt) analyzeStmt(stmt->stmt.get());
}

void SemanticAnalyzer::analyzeBreakStmt(ast::BreakStmt* stmt) {
    if (!stmt) return;
    if (loopDepth_ == 0) {
        error(stmt->location, "'break' 语句不在循环或 switch 语句中");
    }
}

void SemanticAnalyzer::analyzeContinueStmt(ast::ContinueStmt* stmt) {
    if (!stmt) return;
    if (loopDepth_ == 0 || (switchDepth_ > 0 && loopDepth_ <= switchDepth_)) {
        error(stmt->location, "'continue' 语句不在循环中");
    }
}

void SemanticAnalyzer::analyzeReturnStmt(ast::ReturnStmt* stmt) {
    if (!stmt) return;
    
    TypePtr expectedType = symTable_.getCurrentReturnType();
    
    if (stmt->value) {
        TypePtr actualType = analyzeExpr(stmt->value.get());
        if (expectedType && expectedType->isVoid()) {
            error(stmt->location, "void 函数不应该返回值");
        } else if (actualType && expectedType && !canImplicitlyConvert(actualType, expectedType)) {
            error(stmt->location, "返回类型不兼容");
        }
    } else {
        if (expectedType && !expectedType->isVoid()) {
            warning(stmt->location, "非 void 函数应该返回一个值");
        }
    }
}

//=============================================================================
// 表达式处理
//=============================================================================

TypePtr SemanticAnalyzer::analyzeExpr(ast::Expr* expr) {
    if (!expr) return nullptr;
    
    TypePtr result = nullptr;
    
    if (auto intLit = dynamic_cast<ast::IntLiteral*>(expr)) {
        result = analyzeIntLiteral(intLit);
    } else if (auto floatLit = dynamic_cast<ast::FloatLiteral*>(expr)) {
        result = analyzeFloatLiteral(floatLit);
    } else if (dynamic_cast<ast::CharLiteral*>(expr)) {
        result = makeChar();
    } else if (dynamic_cast<ast::StringLiteral*>(expr)) {
        result = makePointer(makeChar());
    } else if (auto identExpr = dynamic_cast<ast::IdentExpr*>(expr)) {
        result = analyzeIdentExpr(identExpr);
    } else if (auto binaryExpr = dynamic_cast<ast::BinaryExpr*>(expr)) {
        result = analyzeBinaryExpr(binaryExpr);
    } else if (auto unaryExpr = dynamic_cast<ast::UnaryExpr*>(expr)) {
        result = analyzeUnaryExpr(unaryExpr);
    } else if (auto callExpr = dynamic_cast<ast::CallExpr*>(expr)) {
        result = analyzeCallExpr(callExpr);
    } else if (auto subscriptExpr = dynamic_cast<ast::SubscriptExpr*>(expr)) {
        result = analyzeSubscriptExpr(subscriptExpr);
    } else if (auto memberExpr = dynamic_cast<ast::MemberExpr*>(expr)) {
        result = analyzeMemberExpr(memberExpr);
    } else if (auto castExpr = dynamic_cast<ast::CastExpr*>(expr)) {
        result = analyzeCastExpr(castExpr);
    } else if (dynamic_cast<ast::SizeofTypeExpr*>(expr)) {
        result = makeLong(true);
    } else if (auto condExpr = dynamic_cast<ast::ConditionalExpr*>(expr)) {
        result = analyzeConditionalExpr(condExpr);
    } else if (auto initList = dynamic_cast<ast::InitListExpr*>(expr)) {
        for (auto& elem : initList->elements) analyzeExpr(elem.get());
        result = nullptr;
    }
    
    if (result && expr) {
        expr->exprType = convertToAstType(result);
    }
    
    return result;
}

TypePtr SemanticAnalyzer::analyzeIntLiteral(ast::IntLiteral* expr) {
    if (!expr) return nullptr;
    
    if (expr->isUnsigned) {
        if (expr->isLongLong) return makeLongLong(true);
        if (expr->isLong) return makeLong(true);
        return makeInt(true);
    } else {
        if (expr->isLongLong) return makeLongLong(false);
        if (expr->isLong) return makeLong(false);
        return makeInt(false);
    }
}

TypePtr SemanticAnalyzer::analyzeFloatLiteral(ast::FloatLiteral* expr) {
    if (!expr) return nullptr;
    return expr->isFloat ? makeFloat() : makeDouble();
}

TypePtr SemanticAnalyzer::analyzeIdentExpr(ast::IdentExpr* expr) {
    if (!expr) return nullptr;
    
    auto sym = symTable_.lookup(expr->name);
    if (!sym) {
        error(expr->location, "未声明的标识符 '" + expr->name + "'");
        return nullptr;
    }
    
    expr->isLValue = (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter);
    return sym->type;
}

TypePtr SemanticAnalyzer::analyzeBinaryExpr(ast::BinaryExpr* expr) {
    if (!expr) return nullptr;
    
    TypePtr leftType = analyzeExpr(expr->left.get());
    TypePtr rightType = analyzeExpr(expr->right.get());
    
    if (!leftType || !rightType) return nullptr;
    
    using Op = ast::BinaryOp;
    
    switch (expr->op) {
        case Op::Assign:
        case Op::AddAssign: case Op::SubAssign:
        case Op::MulAssign: case Op::DivAssign: case Op::ModAssign:
        case Op::AndAssign: case Op::OrAssign: case Op::XorAssign:
        case Op::ShlAssign: case Op::ShrAssign:
            if (!checkAssignable(expr->left.get())) {
                error(expr->location, "表达式不可赋值");
            }
            if (!canImplicitlyConvert(rightType, leftType)) {
                error(expr->location, "赋值语句中的类型不兼容");
            }
            return leftType;
        
        case Op::Add:
        case Op::Sub:
            if (leftType->isPointer() && rightType->isInteger()) return leftType;
            if (leftType->isInteger() && rightType->isPointer()) return rightType;
            if (expr->op == Op::Sub && leftType->isPointer() && rightType->isPointer()) {
                return makeLong();
            }
            return getCommonType(leftType, rightType);
        
        case Op::Mul:
        case Op::Div:
            if (!leftType->isArithmetic() || !rightType->isArithmetic()) {
                error(expr->location, "二元运算符的操作数类型无效");
                return nullptr;
            }
            return getCommonType(leftType, rightType);
        
        case Op::Mod:
            if (!leftType->isInteger() || !rightType->isInteger()) {
                error(expr->location, "取模运算符的操作数类型无效");
                return nullptr;
            }
            return getCommonType(leftType, rightType);
        
        case Op::BitAnd: case Op::BitOr: case Op::BitXor:
        case Op::Shl: case Op::Shr:
            if (!leftType->isInteger() || !rightType->isInteger()) {
                error(expr->location, "位运算符的操作数必须是整数类型");
                return nullptr;
            }
            return getCommonType(leftType, rightType);
        
        case Op::Eq: case Op::Ne:
        case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge:
            return makeInt();
        
        case Op::LogAnd:
        case Op::LogOr:
            return makeInt();
        
        case Op::Comma:
            return rightType;
        
        default:
            return nullptr;
    }
}

TypePtr SemanticAnalyzer::analyzeUnaryExpr(ast::UnaryExpr* expr) {
    if (!expr) return nullptr;
    
    TypePtr operandType = analyzeExpr(expr->operand.get());
    if (!operandType) return nullptr;
    
    using Op = ast::UnaryOp;
    
    switch (expr->op) {
        case Op::Plus:
            if (!operandType->isArithmetic()) {
                error(expr->location, "一元 + 运算符的操作数类型无效");
                return nullptr;
            }
            return performIntegralPromotions(operandType);
        
        case Op::Minus:
            if (!operandType->isArithmetic()) {
                error(expr->location, "一元 - 运算符的操作数类型无效");
                return nullptr;
            }
            return performIntegralPromotions(operandType);
        
        case Op::Not:
            return makeInt();
        
        case Op::BitNot:
            if (!operandType->isInteger()) {
                error(expr->location, "~ 运算符的操作数类型无效");
                return nullptr;
            }
            return performIntegralPromotions(operandType);
        
        case Op::PreInc: case Op::PreDec:
        case Op::PostInc: case Op::PostDec:
            if (!checkAssignable(expr->operand.get())) {
                error(expr->location, "表达式不可赋值");
            }
            return operandType;
        
        case Op::Deref:
            if (!operandType->isPointer()) {
                error(expr->location, "解引用运算符需要指针类型操作数");
                return nullptr;
            }
            expr->isLValue = true;
            return static_cast<PointerType*>(operandType.get())->pointee->clone();
        
        case Op::AddrOf:
            if (!checkAssignable(expr->operand.get())) {
                warning(expr->location, "取临时值的地址");
            }
            return makePointer(operandType);
        
        case Op::Sizeof:
            return makeLong(true);
        
        default:
            return nullptr;
    }
}

TypePtr SemanticAnalyzer::analyzeCallExpr(ast::CallExpr* expr) {
    if (!expr) return nullptr;
    
    TypePtr calleeType = analyzeExpr(expr->callee.get());
    if (!calleeType) return nullptr;
    
    if (calleeType->isPointer()) {
        auto ptr = static_cast<PointerType*>(calleeType.get());
        if (ptr->pointee->isFunction()) {
            calleeType = ptr->pointee->clone();
        }
    }
    
    if (!calleeType->isFunction()) {
        error(expr->location, "被调用的对象不是函数");
        return nullptr;
    }
    
    auto funcType = static_cast<FunctionType*>(calleeType.get());
    
    size_t expectedParams = funcType->paramTypes.size();
    size_t actualArgs = expr->arguments.size();
    
    if (!funcType->isVariadic && actualArgs != expectedParams) {
        error(expr->location, "参数数量不正确");
    } else if (funcType->isVariadic && actualArgs < expectedParams) {
        error(expr->location, "参数太少");
    }
    
    for (size_t i = 0; i < expr->arguments.size(); ++i) {
        TypePtr argType = analyzeExpr(expr->arguments[i].get());
        if (i < expectedParams && argType) {
            if (!canImplicitlyConvert(argType, funcType->paramTypes[i])) {
                error(expr->location, "参数类型不兼容");
            }
        }
    }
    
    return funcType->returnType->clone();
}

TypePtr SemanticAnalyzer::analyzeSubscriptExpr(ast::SubscriptExpr* expr) {
    if (!expr) return nullptr;
    
    TypePtr baseType = analyzeExpr(expr->array.get());
    TypePtr indexType = analyzeExpr(expr->index.get());
    
    if (!baseType || !indexType) return nullptr;
    
    TypePtr elementType = nullptr;
    
    // 正常情况：array[index] 或 pointer[index]
    if (baseType->isArray()) {
        elementType = static_cast<ArrayType*>(baseType.get())->elementType->clone();
        if (!indexType->isInteger()) {
            error(expr->location, "数组下标必须是整数");
        }
    } else if (baseType->isPointer()) {
        elementType = static_cast<PointerType*>(baseType.get())->pointee->clone();
        if (!indexType->isInteger()) {
            error(expr->location, "数组下标必须是整数");
        }
    } 
    // C 语言特性：index[array] 等同于 array[index]
    else if (baseType->isInteger()) {
        if (indexType->isArray()) {
            elementType = static_cast<ArrayType*>(indexType.get())->elementType->clone();
        } else if (indexType->isPointer()) {
            elementType = static_cast<PointerType*>(indexType.get())->pointee->clone();
        } else {
            error(expr->location, "下标运算符需要数组或指针");
            return nullptr;
        }
    } else {
        error(expr->location, "下标运算符需要数组或指针");
        return nullptr;
    }
    
    expr->isLValue = true;
    return elementType;
}

TypePtr SemanticAnalyzer::analyzeMemberExpr(ast::MemberExpr* expr) {
    if (!expr) return nullptr;
    
    TypePtr baseType = analyzeExpr(expr->object.get());
    if (!baseType) return nullptr;
    
    if (expr->isArrow) {
        if (!baseType->isPointer()) {
            error(expr->location, "成员引用的类型不是指针");
            return nullptr;
        }
        baseType = static_cast<PointerType*>(baseType.get())->pointee->clone();
    }
    
    const Member* member = nullptr;
    if (baseType->isStruct()) {
        member = static_cast<StructType*>(baseType.get())->findMember(expr->member);
    } else if (baseType->isUnion()) {
        member = static_cast<UnionType*>(baseType.get())->findMember(expr->member);
    } else {
        error(expr->location, "成员引用的基类型不是结构体或联合体");
        return nullptr;
    }
    
    if (!member) {
        error(expr->location, "没有名为 '" + expr->member + "' 的成员");
        return nullptr;
    }
    
    expr->isLValue = true;
    return member->type->clone();
}

TypePtr SemanticAnalyzer::analyzeCastExpr(ast::CastExpr* expr) {
    if (!expr) return nullptr;
    analyzeExpr(expr->operand.get());
    return resolveAstType(expr->targetType.get());
}

TypePtr SemanticAnalyzer::analyzeConditionalExpr(ast::ConditionalExpr* expr) {
    if (!expr) return nullptr;
    
    TypePtr condType = analyzeExpr(expr->condition.get());
    TypePtr trueType = analyzeExpr(expr->thenExpr.get());
    TypePtr falseType = analyzeExpr(expr->elseExpr.get());
    
    checkCondition(condType, expr->location);
    
    if (!trueType || !falseType) return nullptr;
    return getCommonType(trueType, falseType);
}

//=============================================================================
// 类型转换
//=============================================================================

TypePtr SemanticAnalyzer::resolveAstType(ast::Type* astType) {
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
        TypePtr pointee = resolveAstType(ptrType->pointee.get());
        return pointee ? makePointer(pointee) : nullptr;
    } else if (auto arrType = dynamic_cast<ast::ArrayType*>(astType)) {
        TypePtr elemType = resolveAstType(arrType->elementType.get());
        int len = -1;
        return elemType ? makeArray(elemType, len) : nullptr;
    } else if (auto funcType = dynamic_cast<ast::FunctionType*>(astType)) {
        TypePtr retType = resolveAstType(funcType->returnType.get());
        auto ft = std::make_shared<FunctionType>(retType ? retType : makeInt());
        ft->isVariadic = funcType->isVariadic;
        for (const auto& pt : funcType->paramTypes) {
            TypePtr paramType = resolveAstType(pt.get());
            ft->paramTypes.push_back(paramType ? paramType : makeInt());
        }
        return ft;
    } else if (auto recordType = dynamic_cast<ast::RecordType*>(astType)) {
        // 首先检查是否是带字段的结构体定义
        if (!recordType->fields.empty()) {
            // 这是一个结构体定义，需要创建并注册类型
            if (recordType->isUnion) {
                auto ut = std::make_shared<UnionType>(recordType->name);
                for (const auto& field : recordType->fields) {
                    TypePtr fieldType = resolveAstType(field->type.get());
                    if (fieldType) {
                        ut->members.push_back({field->name, fieldType, 0});
                    }
                }
                ut->isComplete = true;
                if (!recordType->name.empty()) {
                    auto sym = std::make_shared<Symbol>(
                        recordType->name, SymbolKind::UnionTag, ut, SourceLocation{});
                    symTable_.addTag(sym);
                }
                return ut;
            } else {
                auto st = std::make_shared<StructType>(recordType->name);
                int offset = 0;
                for (const auto& field : recordType->fields) {
                    TypePtr fieldType = resolveAstType(field->type.get());
                    if (fieldType) {
                        int align = fieldType->alignment();
                        offset = (offset + align - 1) / align * align;
                        
                        // 处理匿名结构体/联合体：将其成员提升到父结构体
                        if (field->name.empty() && (fieldType->isStruct() || fieldType->isUnion())) {
                            // 获取匿名结构体/联合体的成员
                            if (fieldType->isStruct()) {
                                auto anon = static_cast<StructType*>(fieldType.get());
                                for (const auto& member : anon->members) {
                                    st->members.push_back({member.name, member.type->clone(), offset + member.offset});
                                }
                            } else {
                                auto anon = static_cast<UnionType*>(fieldType.get());
                                for (const auto& member : anon->members) {
                                    st->members.push_back({member.name, member.type->clone(), offset});
                                }
                            }
                        } else {
                            st->members.push_back({field->name, fieldType, offset});
                        }
                        offset += fieldType->size();
                    }
                }
                st->isComplete = true;
                if (!recordType->name.empty()) {
                    auto sym = std::make_shared<Symbol>(
                        recordType->name, SymbolKind::StructTag, st, SourceLocation{});
                    symTable_.addTag(sym);
                }
                return st;
            }
        }
        
        // 查找已定义的类型
        auto tag = symTable_.lookupTag(recordType->name);
        if (tag) return tag->type->clone();
        
        // 未定义的类型，创建不完整类型
        if (recordType->isUnion) {
            return std::make_shared<UnionType>(recordType->name);
        } else {
            return std::make_shared<StructType>(recordType->name);
        }
    } else if (auto enumType = dynamic_cast<ast::EnumType*>(astType)) {
        // 首先检查是否是带常量的枚举定义
        if (!enumType->constants.empty()) {
            auto et = std::make_shared<EnumType>(enumType->name);
            
            // 先注册枚举标签（用于常量表达式求值时查找已定义的枚举常量）
            if (!enumType->name.empty()) {
                auto tagSym = std::make_shared<Symbol>(
                    enumType->name, SymbolKind::EnumTag, et, SourceLocation{});
                symTable_.addTag(tagSym);
            }
            
            int64_t nextValue = 0;
            for (const auto& constant : enumType->constants) {
                if (constant->value) {
                    int64_t constVal;
                    if (evaluateConstantExpr(constant->value.get(), constVal)) {
                        nextValue = constVal;
                    }
                }
                et->enumerators[constant->name] = nextValue;
                
                // 注册枚举常量到符号表
                auto sym = std::make_shared<Symbol>(
                    constant->name, SymbolKind::EnumConstant, makeInt(), constant->location);
                symTable_.addSymbol(sym);
                
                nextValue++;
            }
            
            return et;
        }
        
        auto tag = symTable_.lookupTag(enumType->name);
        if (tag) return tag->type->clone();
        return std::make_shared<EnumType>(enumType->name);
    } else if (auto typedefType = dynamic_cast<ast::TypedefType*>(astType)) {
        auto sym = symTable_.lookup(typedefType->name);
        if (sym && sym->kind == SymbolKind::TypeDef) {
            return sym->type->clone();
        }
        error(SourceLocation{}, "未知的类型名 '" + typedefType->name + "'");
        return nullptr;
    }
    
    return nullptr;
}

ast::TypePtr SemanticAnalyzer::convertToAstType(TypePtr semType) {
    if (!semType) return nullptr;
    
    if (semType->isVoid()) {
        return std::make_unique<ast::BasicType>(ast::BasicTypeKind::Void);
    } else if (semType->isInteger()) {
        auto intType = static_cast<IntegerType*>(semType.get());
        ast::BasicTypeKind kind = ast::BasicTypeKind::Int;
        switch (intType->intKind) {
            case IntegerKind::Char:
                kind = intType->isUnsigned ? ast::BasicTypeKind::UChar : ast::BasicTypeKind::Char;
                break;
            case IntegerKind::Short:
                kind = intType->isUnsigned ? ast::BasicTypeKind::UShort : ast::BasicTypeKind::Short;
                break;
            case IntegerKind::Int:
                kind = intType->isUnsigned ? ast::BasicTypeKind::UInt : ast::BasicTypeKind::Int;
                break;
            case IntegerKind::Long:
                kind = intType->isUnsigned ? ast::BasicTypeKind::ULong : ast::BasicTypeKind::Long;
                break;
            case IntegerKind::LongLong:
                kind = intType->isUnsigned ? ast::BasicTypeKind::ULongLong : ast::BasicTypeKind::LongLong;
                break;
        }
        return std::make_unique<ast::BasicType>(kind);
    } else if (semType->isFloat()) {
        auto floatType = static_cast<FloatType*>(semType.get());
        ast::BasicTypeKind kind = ast::BasicTypeKind::Double;
        switch (floatType->floatKind) {
            case FloatKind::Float: kind = ast::BasicTypeKind::Float; break;
            case FloatKind::Double: kind = ast::BasicTypeKind::Double; break;
            case FloatKind::LongDouble: kind = ast::BasicTypeKind::LongDouble; break;
        }
        return std::make_unique<ast::BasicType>(kind);
    } else if (semType->isPointer()) {
        auto ptrType = static_cast<PointerType*>(semType.get());
        return std::make_unique<ast::PointerType>(convertToAstType(ptrType->pointee));
    } else if (semType->isStruct()) {
        auto structType = static_cast<StructType*>(semType.get());
        return std::make_unique<ast::RecordType>(false, structType->name);
    } else if (semType->isUnion()) {
        auto unionType = static_cast<UnionType*>(semType.get());
        return std::make_unique<ast::RecordType>(true, unionType->name);
    } else if (semType->isArray()) {
        auto arrType = static_cast<ArrayType*>(semType.get());
        auto elemAstType = convertToAstType(arrType->elementType);
        auto result = std::make_unique<ast::ArrayType>(std::move(elemAstType), nullptr);
        if (arrType->length >= 0) {
            result->size = std::make_unique<ast::IntLiteral>(SourceLocation{}, arrType->length);
        }
        return result;
    } else if (semType->isEnum()) {
        auto enumType = static_cast<EnumType*>(semType.get());
        return std::make_unique<ast::EnumType>(enumType->name);
    } else if (semType->isFunction()) {
        auto funcType = static_cast<FunctionType*>(semType.get());
        auto retAstType = convertToAstType(funcType->returnType);
        auto result = std::make_unique<ast::FunctionType>(std::move(retAstType));
        result->isVariadic = funcType->isVariadic;
        for (const auto& paramType : funcType->paramTypes) {
            result->paramTypes.push_back(convertToAstType(paramType));
        }
        return result;
    }
    
    return nullptr;
}

//=============================================================================
// 辅助函数
//=============================================================================

bool SemanticAnalyzer::checkAssignable(ast::Expr* expr) {
    if (!expr) return false;
    return expr->isLValue;
}

bool SemanticAnalyzer::checkCondition(TypePtr type, const SourceLocation& loc) {
    if (!type) return false;
    if (!type->isScalar()) {
        error(loc, "条件表达式必须是标量类型");
        return false;
    }
    return true;
}

TypePtr SemanticAnalyzer::performIntegralPromotions(TypePtr type) {
    if (!type || !type->isInteger()) return type;
    
    auto intType = static_cast<IntegerType*>(type.get());
    if (intType->intKind < IntegerKind::Int) {
        return makeInt(false);
    }
    return type->clone();
}

/**
 * @brief 求值整数常量表达式
 * 
 * 支持以下表达式类型：
 * - 整数字面量
 * - 字符字面量
 * - 枚举常量
 * - 一元运算符（+、-、~、!）
 * - 二元运算符（+、-、*、/、%、&、|、^、<<、>>）
 * - 条件表达式（?:）
 * - 括号表达式
 * 
 * @param expr 表达式 AST
 * @param[out] result 计算结果
 * @return 成功求值返回 true
 */
bool SemanticAnalyzer::evaluateConstantExpr(ast::Expr* expr, int64_t& result) {
    if (!expr) return false;
    
    // 整数字面量
    if (auto intLit = dynamic_cast<ast::IntLiteral*>(expr)) {
        result = intLit->value;
        return true;
    }
    
    // 字符字面量
    if (auto charLit = dynamic_cast<ast::CharLiteral*>(expr)) {
        result = static_cast<int64_t>(charLit->value);
        return true;
    }
    
    // 标识符（枚举常量）
    if (auto identExpr = dynamic_cast<ast::IdentExpr*>(expr)) {
        auto sym = symTable_.lookup(identExpr->name);
        if (sym && sym->kind == SymbolKind::EnumConstant) {
            // 枚举常量的值存储在 Symbol 的 enumValue 字段
            // 如果没有，尝试遍历所有枚举标签查找
            for (const auto& [tagName, tagSym] : symTable_.getAllTags()) {
                if (tagSym->type && tagSym->type->isEnum()) {
                    auto enumType = static_cast<EnumType*>(tagSym->type.get());
                    auto it = enumType->enumerators.find(identExpr->name);
                    if (it != enumType->enumerators.end()) {
                        result = it->second;
                        return true;
                    }
                }
            }
            // 枚举常量尚未注册到枚举类型中，返回失败
            return false;
        }
        return false;
    }
    
    // 一元表达式
    if (auto unaryExpr = dynamic_cast<ast::UnaryExpr*>(expr)) {
        int64_t operand;
        if (!evaluateConstantExpr(unaryExpr->operand.get(), operand)) {
            return false;
        }
        
        using Op = ast::UnaryOp;
        switch (unaryExpr->op) {
            case Op::Plus:   result = operand; return true;
            case Op::Minus:  result = -operand; return true;
            case Op::BitNot: result = ~operand; return true;
            case Op::Not:    result = !operand; return true;
            default: return false;
        }
    }
    
    // 二元表达式
    if (auto binaryExpr = dynamic_cast<ast::BinaryExpr*>(expr)) {
        int64_t left, right;
        if (!evaluateConstantExpr(binaryExpr->left.get(), left)) {
            return false;
        }
        if (!evaluateConstantExpr(binaryExpr->right.get(), right)) {
            return false;
        }
        
        using Op = ast::BinaryOp;
        switch (binaryExpr->op) {
            case Op::Add:    result = left + right; return true;
            case Op::Sub:    result = left - right; return true;
            case Op::Mul:    result = left * right; return true;
            case Op::Div:
                if (right == 0) return false;
                result = left / right;
                return true;
            case Op::Mod:
                if (right == 0) return false;
                result = left % right;
                return true;
            case Op::BitAnd: result = left & right; return true;
            case Op::BitOr:  result = left | right; return true;
            case Op::BitXor: result = left ^ right; return true;
            case Op::Shl:    result = left << right; return true;
            case Op::Shr:    result = left >> right; return true;
            case Op::Eq:     result = left == right; return true;
            case Op::Ne:     result = left != right; return true;
            case Op::Lt:     result = left < right; return true;
            case Op::Le:     result = left <= right; return true;
            case Op::Gt:     result = left > right; return true;
            case Op::Ge:     result = left >= right; return true;
            case Op::LogAnd: result = left && right; return true;
            case Op::LogOr:  result = left || right; return true;
            default: return false;
        }
    }
    
    // 条件表达式
    if (auto condExpr = dynamic_cast<ast::ConditionalExpr*>(expr)) {
        int64_t cond;
        if (!evaluateConstantExpr(condExpr->condition.get(), cond)) {
            return false;
        }
        return evaluateConstantExpr(
            cond ? condExpr->thenExpr.get() : condExpr->elseExpr.get(),
            result
        );
    }
    
    // 类型转换表达式
    if (auto castExpr = dynamic_cast<ast::CastExpr*>(expr)) {
        return evaluateConstantExpr(castExpr->operand.get(), result);
    }
    
    return false;
}

} // namespace semantic
} // namespace cdd
