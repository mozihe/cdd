/**
 * @file Parser.cpp
 * @brief 语法分析器实现
 * @author mozihe
 */

#include "Parser.h"
#include <sstream>
#include <cassert>

namespace cdd {

Parser::Parser(Lexer& lexer) : lexer_(lexer) {
    advance();
}

Token Parser::peek() {
    if (!hasLookahead_) {
        lookahead_ = lexer_.nextToken();
        hasLookahead_ = true;
    }
    return lookahead_;
}

void Parser::advance() {
    if (hasLookahead_) {
        currentToken_ = lookahead_;
        hasLookahead_ = false;
    } else {
        currentToken_ = lexer_.nextToken();
    }
}

bool Parser::check(TokenKind kind) {
    return current().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(TokenKind kind, const std::string& message) {
    if (check(kind)) {
        Token tok = current();
        advance();
        return tok;
    }
    error(message);
    return Token(TokenKind::Invalid, current().location);
}

bool Parser::isAtEnd() {
    return check(TokenKind::EndOfFile);
}

//=============================================================================
// 错误处理
//=============================================================================

void Parser::error(const std::string& message) {
    errorAt(current(), message);
}

void Parser::errorAt(const Token& token, const std::string& message) {
    std::ostringstream oss;
    oss << "第 " << token.location.line << " 行，第 " << token.location.column << " 列: "
        << message;
    if (!token.text.empty()) {
        oss << " (在 '" << token.text << "' 处)";
    }
    errors_.emplace_back(oss.str(), token.location);
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (current().kind == TokenKind::Semicolon) {
            advance();
            return;
        }
        switch (current().kind) {
            case TokenKind::KW_if:
            case TokenKind::KW_while:
            case TokenKind::KW_for:
            case TokenKind::KW_return:
            case TokenKind::KW_int:
            case TokenKind::KW_char:
            case TokenKind::KW_void:
            case TokenKind::KW_struct:
            case TokenKind::KW_enum:
            case TokenKind::KW_typedef:
                return;
            default:
                break;
        }
        advance();
    }
}

//=============================================================================
// 辅助判断
//=============================================================================

bool Parser::isTypeName() {
    TokenKind k = current().kind;

    // 类型限定符 (const, volatile, restrict)
    if (k == TokenKind::KW_const || k == TokenKind::KW_volatile ||
        k == TokenKind::KW_restrict) {
        return true;
    }

    // 基本类型关键字
    if (k == TokenKind::KW_void || k == TokenKind::KW_char ||
        k == TokenKind::KW_short || k == TokenKind::KW_int ||
        k == TokenKind::KW_long || k == TokenKind::KW_float ||
        k == TokenKind::KW_double || k == TokenKind::KW_signed ||
        k == TokenKind::KW_unsigned) {
        return true;
    }

    // struct/union/enum
    if (k == TokenKind::KW_struct || k == TokenKind::KW_union ||
        k == TokenKind::KW_enum) {
        return true;
    }

    // typedef 名称
    if (k == TokenKind::Identifier) {
        return typedefNames_.count(std::string(current().text)) > 0;
    }

    return false;
}

bool Parser::isDeclarationSpecifier() {
    return isStorageClassSpecifier() || isTypeSpecifier() || isTypeQualifier();
}

bool Parser::isTypeSpecifier() {
    TokenKind k = current().kind;
    return k == TokenKind::KW_void || k == TokenKind::KW_char ||
           k == TokenKind::KW_short || k == TokenKind::KW_int ||
           k == TokenKind::KW_long || k == TokenKind::KW_float ||
           k == TokenKind::KW_double || k == TokenKind::KW_signed ||
           k == TokenKind::KW_unsigned || k == TokenKind::KW_struct ||
           k == TokenKind::KW_union || k == TokenKind::KW_enum ||
           (k == TokenKind::Identifier && typedefNames_.count(std::string(current().text)) > 0);
}

bool Parser::isTypeQualifier() {
    TokenKind k = current().kind;
    return k == TokenKind::KW_const || k == TokenKind::KW_volatile ||
           k == TokenKind::KW_restrict;
}

bool Parser::isStorageClassSpecifier() {
    TokenKind k = current().kind;
    return k == TokenKind::KW_typedef || k == TokenKind::KW_extern ||
           k == TokenKind::KW_static || k == TokenKind::KW_auto ||
           k == TokenKind::KW_register;
}

//=============================================================================
// 翻译单元
//=============================================================================

/**
 * @brief 解析整个翻译单元
 * 
 * 处理流程：
 * 1. 循环解析顶层声明直到文件结束
 * 2. 每个声明可能产生多个 AST 节点（如 int a, b, c;）
 * 3. 遇到错误时进行错误恢复，继续解析
 */
std::unique_ptr<ast::TranslationUnit> Parser::parseTranslationUnit() {
    auto unit = std::make_unique<ast::TranslationUnit>();

    // 循环解析顶层声明
    while (!isAtEnd()) {
        try {
            auto decls = parseDeclaration();
            for (auto& decl : decls) {
                if (decl) {
                    unit->declarations.push_back(std::move(decl));
                }
            }
        } catch (const ParseError& e) {
            synchronize();  // 错误恢复
        }
    }

    return unit;
}

//=============================================================================
// 声明解析
//=============================================================================

/**
 * @brief 解析声明
 * 
 * 处理流程：
 * 1. 解析声明说明符（存储类、类型限定符、类型说明符）
 * 2. 检查是否仅有类型说明符（如 struct 定义）
 * 3. 解析声明符（名称、指针、数组、函数）
 * 4. 根据声明符类型分发：
 *    - typedef 声明
 *    - 函数定义/原型
 *    - 变量声明（可能有多个）
 */
ast::DeclList Parser::parseDeclaration() {
    ast::DeclList result;
    SourceLocation startLoc = current().location;

    // 1. 解析声明说明符
    DeclSpec spec = parseDeclarationSpecifiers();

    if (!spec.type) {
        error("需要类型说明符");
        return result;
    }

    // 2. 检查是否仅有类型说明符（如 struct 定义）
    if (check(TokenKind::Semicolon)) {
        advance();
        // 如果是结构体/联合体定义，创建 RecordDecl
        if (auto* recordType = dynamic_cast<ast::RecordType*>(spec.type.get())) {
            if (!recordType->fields.empty()) {
                auto recordDecl = std::make_unique<ast::RecordDecl>(
                    startLoc, recordType->isUnion, recordType->name);
                recordDecl->fields = std::move(recordType->fields);
                result.push_back(std::move(recordDecl));
                return result;
            }
        }
        // 如果是枚举定义，创建 EnumDecl
        if (auto* enumType = dynamic_cast<ast::EnumType*>(spec.type.get())) {
            if (!enumType->constants.empty()) {
                auto enumDecl = std::make_unique<ast::EnumDecl>(startLoc, enumType->name);
                enumDecl->constants = std::move(enumType->constants);
                result.push_back(std::move(enumDecl));
                return result;
            }
        }
        // 返回空声明列表
        return result;
    }

    // 解析声明符
    Declarator decl = parseDeclarator(spec.type ? spec.type->clone() : nullptr);

    // typedef 声明 - 优先处理，因为 typedef 函数类型也会设置 isFunction
    if (spec.isTypedef) {
        typedefNames_.insert(decl.name);
        auto td = std::make_unique<ast::TypedefDecl>(
            startLoc, decl.name, std::move(decl.type));
        td->storage = ast::StorageClass::Typedef;
        result.push_back(std::move(td));

        // 处理更多 typedef 声明符
        while (match(TokenKind::Comma)) {
            Declarator nextDecl = parseDeclarator(spec.type ? spec.type->clone() : nullptr);
            typedefNames_.insert(nextDecl.name);
            auto nextTd = std::make_unique<ast::TypedefDecl>(
                nextDecl.location.line > 0 ? nextDecl.location : startLoc,
                nextDecl.name, std::move(nextDecl.type));
            nextTd->storage = ast::StorageClass::Typedef;
            result.push_back(std::move(nextTd));
        }

        expect(TokenKind::Semicolon, "声明后需要 ';'");
        return result;
    }

    // 函数定义：有函数体 { ... }
    if (decl.isFunction && check(TokenKind::LeftBrace)) {
        // decl.type 是 FunctionType，需要提取 returnType
        ast::TypePtr returnType;
        if (auto* funcType = dynamic_cast<ast::FunctionType*>(decl.type.get())) {
            returnType = std::move(funcType->returnType);
        } else {
            returnType = std::move(decl.type);
        }
        
        auto funcDecl = std::make_unique<ast::FunctionDecl>(
            startLoc, decl.name, std::move(returnType));
        funcDecl->params = std::move(decl.params);
        funcDecl->isVariadic = decl.isVariadic;
        funcDecl->body = parseCompoundStatement();
        funcDecl->storage = spec.storage;
        result.push_back(std::move(funcDecl));
        return result;
    }

    // 函数原型：没有函数体，以 ; 结束
    if (decl.isFunction) {
        // decl.type 是 FunctionType，需要提取 returnType
        ast::TypePtr returnType;
        if (auto* funcType = dynamic_cast<ast::FunctionType*>(decl.type.get())) {
            returnType = std::move(funcType->returnType);
        } else {
            returnType = std::move(decl.type);
        }
        
        auto funcDecl = std::make_unique<ast::FunctionDecl>(
            startLoc, decl.name, std::move(returnType));
        funcDecl->params = std::move(decl.params);
        funcDecl->isVariadic = decl.isVariadic;
        funcDecl->body = nullptr;  // 只是声明，没有定义
        funcDecl->storage = spec.storage;
        result.push_back(std::move(funcDecl));
        
        expect(TokenKind::Semicolon, "函数声明后需要 ';'");
        return result;
    }

    // 普通变量声明
    ast::ExprPtr init;
    if (match(TokenKind::Equal)) {
        init = parseInitializer();
    }

    auto varDecl = std::make_unique<ast::VarDecl>(
        startLoc, decl.name, std::move(decl.type), std::move(init));
    varDecl->storage = spec.storage;
    result.push_back(std::move(varDecl));

    // 处理更多声明符
    while (match(TokenKind::Comma)) {
        Declarator nextDecl = parseDeclarator(spec.type ? spec.type->clone() : nullptr);

        ast::ExprPtr nextInit;
        if (match(TokenKind::Equal)) {
            nextInit = parseInitializer();
        }
        
        auto nextVarDecl = std::make_unique<ast::VarDecl>(
            nextDecl.location.line > 0 ? nextDecl.location : startLoc,
            nextDecl.name, std::move(nextDecl.type), std::move(nextInit));
        nextVarDecl->storage = spec.storage;
        result.push_back(std::move(nextVarDecl));
    }

    expect(TokenKind::Semicolon, "声明后需要 ';'");
    return result;
}

Parser::DeclSpec Parser::parseDeclarationSpecifiers() {
    DeclSpec spec;
    bool hasType = false;
    bool isSigned = true;
    bool isLong = false;
    bool isLongLong = false;
    bool isShort = false;
    ast::BasicTypeKind baseKind = ast::BasicTypeKind::Int;

    while (true) {
        if (isStorageClassSpecifier()) {
            TokenKind k = current().kind;
            advance();
            switch (k) {
                case TokenKind::KW_typedef:
                    spec.storage = ast::StorageClass::Typedef;
                    spec.isTypedef = true;
                    break;
                case TokenKind::KW_extern:
                    spec.storage = ast::StorageClass::Extern;
                    break;
                case TokenKind::KW_static:
                    spec.storage = ast::StorageClass::Static;
                    break;
                case TokenKind::KW_auto:
                    spec.storage = ast::StorageClass::Auto;
                    break;
                case TokenKind::KW_register:
                    spec.storage = ast::StorageClass::Register;
                    break;
                default:
                    break;
            }
        } else if (isTypeQualifier()) {
            TokenKind k = current().kind;
            advance();
            switch (k) {
                case TokenKind::KW_const:
                    spec.qualifiers = spec.qualifiers | ast::TypeQualifier::Const;
                    break;
                case TokenKind::KW_volatile:
                    spec.qualifiers = spec.qualifiers | ast::TypeQualifier::Volatile;
                    break;
                case TokenKind::KW_restrict:
                    spec.qualifiers = spec.qualifiers | ast::TypeQualifier::Restrict;
                    break;
                default:
                    break;
            }
        } else if (isTypeSpecifier()) {
            TokenKind k = current().kind;

            switch (k) {
                case TokenKind::KW_void:
                    advance();
                    baseKind = ast::BasicTypeKind::Void;
                    hasType = true;
                    break;
                case TokenKind::KW_char:
                    advance();
                    baseKind = ast::BasicTypeKind::Char;
                    hasType = true;
                    break;
                case TokenKind::KW_short:
                    advance();
                    isShort = true;
                    hasType = true;
                    break;
                case TokenKind::KW_int:
                    advance();
                    hasType = true;
                    break;
                case TokenKind::KW_long:
                    advance();
                    if (isLong) {
                        isLongLong = true;
                    }
                    isLong = true;
                    hasType = true;
                    break;
                case TokenKind::KW_float:
                    advance();
                    baseKind = ast::BasicTypeKind::Float;
                    hasType = true;
                    break;
                case TokenKind::KW_double:
                    advance();
                    baseKind = ast::BasicTypeKind::Double;
                    hasType = true;
                    break;
                case TokenKind::KW_signed:
                    advance();
                    isSigned = true;
                    hasType = true;
                    break;
                case TokenKind::KW_unsigned:
                    advance();
                    isSigned = false;
                    hasType = true;
                    break;
                case TokenKind::KW_struct:
                case TokenKind::KW_union:
                    spec.type = parseStructOrUnionSpecifier();
                    hasType = true;
                    goto done;  // struct/union 是完整的类型
                case TokenKind::KW_enum:
                    spec.type = parseEnumSpecifier();
                    hasType = true;
                    goto done;
                case TokenKind::Identifier:
                    // typedef 名称
                    if (typedefNames_.count(std::string(current().text)) > 0) {
                        spec.type = std::make_unique<ast::TypedefType>(std::string(current().text));
                        advance();
                        hasType = true;
                        goto done;
                    }
                    goto done;
                default:
                    goto done;
            }
        } else {
            break;
        }
    }

done:
    if (hasType && !spec.type) {
        // 构建基本类型
        if (isLongLong) {
            baseKind = isSigned ? ast::BasicTypeKind::LongLong : ast::BasicTypeKind::ULongLong;
        } else if (isLong) {
            if (baseKind == ast::BasicTypeKind::Double) {
                baseKind = ast::BasicTypeKind::LongDouble;
            } else {
                baseKind = isSigned ? ast::BasicTypeKind::Long : ast::BasicTypeKind::ULong;
            }
        } else if (isShort) {
            baseKind = isSigned ? ast::BasicTypeKind::Short : ast::BasicTypeKind::UShort;
        } else if (baseKind == ast::BasicTypeKind::Char) {
            if (!isSigned) baseKind = ast::BasicTypeKind::UChar;
        } else if (baseKind == ast::BasicTypeKind::Int) {
            if (!isSigned) baseKind = ast::BasicTypeKind::UInt;
        }

        spec.type = std::make_unique<ast::BasicType>(baseKind);
    }

    if (spec.type) {
        spec.type->qualifiers = spec.qualifiers;
    }

    return spec;
}

ast::TypePtr Parser::parseStructOrUnionSpecifier() {
    bool isUnion = check(TokenKind::KW_union);
    advance();  // 跳过 struct/union

    std::string name;
    if (check(TokenKind::Identifier)) {
        name = std::string(current().text);
        advance();
    }

    if (match(TokenKind::LeftBrace)) {
        // 结构体定义
        auto fields = parseStructDeclarationList();
        expect(TokenKind::RightBrace, "结构体定义后需要 '}'");

        // 创建带字段的记录类型
        auto recordType = std::make_unique<ast::RecordType>(isUnion, name);
        recordType->fields = std::move(fields);
        return recordType;
    }

    // 只是引用
    return std::make_unique<ast::RecordType>(isUnion, name);
}

std::vector<std::unique_ptr<ast::FieldDecl>> Parser::parseStructDeclarationList() {
    std::vector<std::unique_ptr<ast::FieldDecl>> fields;

    while (!check(TokenKind::RightBrace) && !isAtEnd()) {
        SourceLocation loc = current().location;

        // 解析类型
        DeclSpec spec = parseDeclarationSpecifiers();
        if (!spec.type) {
            error("需要类型说明符");
            synchronize();
            continue;
        }

        // 解析声明符
        do {
            Declarator decl = parseDeclarator(spec.type ? spec.type->clone() : nullptr);

            ast::ExprPtr bitWidth;
            if (match(TokenKind::Colon)) {
                bitWidth = parseConditionalExpression();
            }

            fields.push_back(std::make_unique<ast::FieldDecl>(
                loc, decl.name, std::move(decl.type), std::move(bitWidth)));

        } while (match(TokenKind::Comma));

        expect(TokenKind::Semicolon, "成员声明后需要 ';'");
    }

    return fields;
}

ast::TypePtr Parser::parseEnumSpecifier() {
    advance();  // 跳过 enum

    std::string name;
    if (check(TokenKind::Identifier)) {
        name = std::string(current().text);
        advance();
    }

    auto enumType = std::make_unique<ast::EnumType>(name);

    if (match(TokenKind::LeftBrace)) {
        // 枚举定义
        do {
            if (check(TokenKind::Identifier)) {
                SourceLocation constLoc = current().location;
                std::string constName = std::string(current().text);
                advance();

                ast::ExprPtr value;
                if (match(TokenKind::Equal)) {
                    value = parseConditionalExpression();
                }
                
                enumType->constants.push_back(
                    std::make_unique<ast::EnumConstantDecl>(constLoc, constName, std::move(value)));
            }
        } while (match(TokenKind::Comma) && !check(TokenKind::RightBrace));

        expect(TokenKind::RightBrace, "枚举定义后需要 '}'");
    }

    return enumType;
}

Parser::Declarator Parser::parseDeclarator(ast::TypePtr baseType) {
    // 处理指针
    ast::TypePtr type = parsePointer(std::move(baseType));

    // 处理直接声明符
    return parseDirectDeclarator(std::move(type));
}

ast::TypePtr Parser::parsePointer(ast::TypePtr baseType) {
    while (match(TokenKind::Star)) {
        baseType = std::make_unique<ast::PointerType>(std::move(baseType));

        // 解析类型限定符
        while (isTypeQualifier()) {
            if (match(TokenKind::KW_const)) {
                baseType->qualifiers = baseType->qualifiers | ast::TypeQualifier::Const;
            } else if (match(TokenKind::KW_volatile)) {
                baseType->qualifiers = baseType->qualifiers | ast::TypeQualifier::Volatile;
            } else if (match(TokenKind::KW_restrict)) {
                baseType->qualifiers = baseType->qualifiers | ast::TypeQualifier::Restrict;
            }
        }
    }
    return baseType;
}

Parser::Declarator Parser::parseDirectDeclarator(ast::TypePtr baseType) {
    Declarator decl;
    // 保存一份 baseType 副本，因为后面可能需要插入到内部声明符的类型链末端
    ast::TypePtr savedBaseType = baseType ? baseType->clone() : nullptr;
    decl.type = std::move(baseType);
    decl.location = current().location;
    bool hasInnerDeclarator = false;  // 是否有内部声明符（如函数指针）
    ast::TypePtr* innerTypeSlot = nullptr;  // 指向内部类型的"槽"，用于插入后缀类型

    // 首先检查是否有标识符（函数或变量名）
    if (check(TokenKind::Identifier)) {
        decl.name = std::string(current().text);
        decl.location = current().location;
        advance();
    } else if (match(TokenKind::LeftParen)) {
        // ( declarator ) 或 ( 参数列表 )
        // 需要区分函数指针 (*fp) 和无名抽象声明符
        if (isDeclarationSpecifier() || check(TokenKind::RightParen)) {
            // 这是无名抽象声明符（如类型转换中的 (int*)）
            decl.name = "";
            decl.isFunction = true;
            decl.params = std::move(parseParameterTypeList(decl.isVariadic));
            expect(TokenKind::RightParen, "参数列表后需要 ')'");
            return decl;
        }
        // 括号括起来的声明符（如函数指针 (*fp)）
        hasInnerDeclarator = true;
        // 内部声明符独立解析，不带入外层的基类型
        // 外层的 baseType（如 int*）会在后面作为返回类型插入
        Declarator inner = parseDeclarator(nullptr);
        expect(TokenKind::RightParen, "声明符后需要 ')'");
        decl.name = inner.name;
        decl.location = inner.location;
        decl.isFunction = inner.isFunction;  // 传递函数标志
        decl.type = std::move(inner.type);
        decl.params = std::move(inner.params);  // 传递内部声明符的参数（如 get_operator 的参数）
        decl.isVariadic = inner.isVariadic;
        
        // 保留对内部类型链末端的引用，以便后续插入后缀类型
        // 需要找到最内层的指针/数组的 pointee/elementType
        // 对于 int (*foo(int))(int, int)，内部是 FunctionType(PointerType(nullptr))
        // 我们需要找到 PointerType 的 pointee 并将其设为外层的 baseType
        innerTypeSlot = &decl.type;
        while (*innerTypeSlot) {
            if (auto* func = dynamic_cast<ast::FunctionType*>(innerTypeSlot->get())) {
                // 如果是函数类型，深入到返回类型
                innerTypeSlot = &func->returnType;
            } else if (auto* ptr = dynamic_cast<ast::PointerType*>(innerTypeSlot->get())) {
                innerTypeSlot = &ptr->pointee;
            } else if (auto* arr = dynamic_cast<ast::ArrayType*>(innerTypeSlot->get())) {
                innerTypeSlot = &arr->elementType;
            } else {
                break;
            }
        }
        // 将外层的基类型插入到类型链的末端
        if (innerTypeSlot && !*innerTypeSlot && savedBaseType) {
            *innerTypeSlot = std::move(savedBaseType);
        }
    }

    // 后缀：数组和函数
    while (true) {
        if (match(TokenKind::LeftBracket)) {
            // 数组
            ast::ExprPtr size;
            if (!check(TokenKind::RightBracket)) {
                size = parseConditionalExpression();
            }
            expect(TokenKind::RightBracket, "数组声明后需要 ']'");

            if (hasInnerDeclarator && innerTypeSlot) {
                // 对于内部声明符，将数组类型插入到类型链的末端
                *innerTypeSlot = std::make_unique<ast::ArrayType>(std::move(*innerTypeSlot), std::move(size));
                // 更新 innerTypeSlot 指向新插入类型的元素类型
                innerTypeSlot = &static_cast<ast::ArrayType*>(innerTypeSlot->get())->elementType;
            } else {
                decl.type = std::make_unique<ast::ArrayType>(std::move(decl.type), std::move(size));
            }
        } else if (match(TokenKind::LeftParen)) {
            // 函数后缀
            // 只有没有内部声明符时才是真正的函数声明
            // 有内部声明符时（如 int (*fp)(void)），这只是构建函数指针类型
            bool outerIsVariadic = false;
            auto outerParams = parseParameterTypeList(outerIsVariadic);
            expect(TokenKind::RightParen, "参数列表后需要 ')'");
            
            if (!hasInnerDeclarator) {
                // 这是真正的函数声明，参数属于这个函数
                decl.isFunction = true;
                decl.params = std::move(outerParams);
                decl.isVariadic = outerIsVariadic;
            }
            // 如果有内部声明符，decl.params 已经保存了内部函数的参数，不要覆盖

            // 构建函数类型
            ast::TypePtr returnType;
            if (hasInnerDeclarator && innerTypeSlot) {
                returnType = std::move(*innerTypeSlot);
            } else {
                returnType = std::move(decl.type);
            }
            
            auto funcType = std::make_unique<ast::FunctionType>(std::move(returnType));
            funcType->isVariadic = outerIsVariadic;
            // 将参数类型填充到 funcType->paramTypes
            // 使用 outerParams，因为这些是函数指针类型的参数
            for (const auto& param : outerParams) {
                if (param && param->type) {
                    funcType->paramTypes.push_back(param->type->clone());
                }
            }
            
            if (hasInnerDeclarator && innerTypeSlot) {
                // 将函数类型插入到类型链的末端
                *innerTypeSlot = std::move(funcType);
                // 更新 innerTypeSlot 指向新插入函数类型的返回类型
                innerTypeSlot = &static_cast<ast::FunctionType*>(innerTypeSlot->get())->returnType;
            } else {
                decl.type = std::move(funcType);
            }
        } else {
            break;
        }
    }

    return decl;
}

std::vector<std::unique_ptr<ast::ParamDecl>> Parser::parseParameterTypeList(bool& isVariadic) {
    std::vector<std::unique_ptr<ast::ParamDecl>> params;
    isVariadic = false;

    if (check(TokenKind::RightParen)) {
        return params;  // 空参数列表
    }

    // void 单独处理：只有 "void)" 表示无参数，"void*" 等是有参数的
    if (check(TokenKind::KW_void)) {
        // 向前看是否只有 void 一个参数
        Token nextTok = peek();
        if (nextTok.kind == TokenKind::RightParen) {
            advance();  // 消费 void
            return params;  // void 作为无参数标记
        }
        // 否则 void 是参数类型的一部分（如 void*），正常解析
    }

    do {
        if (match(TokenKind::Ellipsis)) {
            isVariadic = true;
            break;
        }

        auto param = parseParameterDeclaration();
        if (param) {
            params.push_back(std::move(param));
        }
    } while (match(TokenKind::Comma));

    return params;
}

std::unique_ptr<ast::ParamDecl> Parser::parseParameterDeclaration() {
    SourceLocation loc = current().location;

    DeclSpec spec = parseDeclarationSpecifiers();
    if (!spec.type) {
        error("需要参数类型");
        return nullptr;
    }

    // 可能有声明符或抽象声明符
    std::string name;
    ast::TypePtr type = spec.type ? std::move(spec.type) : nullptr;

    if (check(TokenKind::Identifier) || check(TokenKind::Star) || check(TokenKind::LeftParen)) {
        Declarator decl = parseDeclarator(std::move(type));
        name = decl.name;
        type = std::move(decl.type);
    }

    return std::make_unique<ast::ParamDecl>(loc, name, std::move(type));
}

ast::TypePtr Parser::parseTypeName() {
    DeclSpec spec = parseDeclarationSpecifiers();
    if (!spec.type) {
        error("需要类型名");
        return nullptr;
    }

    return parseAbstractDeclarator(std::move(spec.type));
}

ast::TypePtr Parser::parseAbstractDeclarator(ast::TypePtr baseType) {
    // 处理指针
    baseType = parsePointer(std::move(baseType));

    // 处理直接抽象声明符
    if (match(TokenKind::LeftParen)) {
        if (isDeclarationSpecifier() || check(TokenKind::RightParen) || check(TokenKind::Ellipsis)) {
            // 函数类型
            bool isVariadic = false;
            auto params = parseParameterTypeList(isVariadic);
            expect(TokenKind::RightParen, "需要 ')'");

            auto funcType = std::make_unique<ast::FunctionType>(std::move(baseType));
            funcType->isVariadic = isVariadic;
            // 将参数类型填充到 funcType->paramTypes
            for (const auto& param : params) {
                if (param && param->type) {
                    funcType->paramTypes.push_back(param->type->clone());
                }
            }
            return funcType;
        }

        // 括号括起来的抽象声明符（如函数指针类型 int (*)(int, int)）
        baseType = parseAbstractDeclarator(std::move(baseType));
        expect(TokenKind::RightParen, "需要 ')'");
    }

    // 处理后缀：函数参数列表或数组
    while (check(TokenKind::LeftParen) || check(TokenKind::LeftBracket)) {
        if (match(TokenKind::LeftParen)) {
            // 函数类型后缀 (int, int)
            bool isVariadic = false;
            auto params = parseParameterTypeList(isVariadic);
            expect(TokenKind::RightParen, "需要 ')'");

            auto funcType = std::make_unique<ast::FunctionType>(std::move(baseType));
            funcType->isVariadic = isVariadic;
            // 将参数类型填充到 funcType->paramTypes
            for (const auto& param : params) {
                if (param && param->type) {
                    funcType->paramTypes.push_back(param->type->clone());
                }
            }
            baseType = std::move(funcType);
        } else if (match(TokenKind::LeftBracket)) {
            // 数组后缀 [size]
            ast::ExprPtr size;
            if (!check(TokenKind::RightBracket)) {
                size = parseConditionalExpression();
            }
            expect(TokenKind::RightBracket, "需要 ']'");

            baseType = std::make_unique<ast::ArrayType>(std::move(baseType), std::move(size));
        }
    }

    return baseType;
}

ast::ExprPtr Parser::parseInitializer() {
    if (check(TokenKind::LeftBrace)) {
        return parseInitializerList();
    }
    return parseAssignmentExpression();
}

ast::ExprPtr Parser::parseInitializerList() {
    SourceLocation loc = current().location;
    expect(TokenKind::LeftBrace, "需要 '{'");

    ast::ExprList elements;

    if (!check(TokenKind::RightBrace)) {
        do {
            // C99 指定初始化器支持
            // 形式1: .member = value (结构体成员)
            // 形式2: [index] = value (数组元素)
            if (check(TokenKind::Dot)) {
                // 结构体成员指定初始化器: .member = value
                SourceLocation designatorLoc = current().location;
                advance(); // 跳过 '.'
                
                if (!check(TokenKind::Identifier)) {
                    error("指定初始化器需要成员名");
                    return nullptr;
                }
                std::string memberName(current().text);
                advance(); // 跳过成员名
                
                expect(TokenKind::Equal, "指定初始化器需要 '='");
                
                auto init = parseInitializer();
                if (init) {
                    // 创建一个特殊的成员初始化表达式
                    // 用 MemberExpr 来表示指定初始化，base 为 nullptr
                    auto designator = std::make_unique<ast::MemberExpr>(
                        designatorLoc,
                        nullptr,  // base 为 null 表示这是指定初始化器
                        memberName,
                        false     // 不是箭头访问
                    );
                    // 用赋值表达式来包装
                    auto assign = std::make_unique<ast::BinaryExpr>(
                        designatorLoc,
                        ast::BinaryOp::Assign,
                        std::move(designator),
                        std::move(init)
                    );
                    elements.push_back(std::move(assign));
                }
            } else if (check(TokenKind::LeftBracket)) {
                // 数组元素指定初始化器: [index] = value
                SourceLocation designatorLoc = current().location;
                advance(); // 跳过 '['
                
                auto indexExpr = parseAssignmentExpression();
                expect(TokenKind::RightBracket, "需要 ']'");
                expect(TokenKind::Equal, "指定初始化器需要 '='");
                
                auto init = parseInitializer();
                if (init && indexExpr) {
                    // 用下标表达式来表示指定初始化，base 为 nullptr
                    auto designator = std::make_unique<ast::SubscriptExpr>(
                        designatorLoc,
                        nullptr,  // base 为 null 表示这是指定初始化器
                        std::move(indexExpr)
                    );
                    // 用赋值表达式来包装
                    auto assign = std::make_unique<ast::BinaryExpr>(
                        designatorLoc,
                        ast::BinaryOp::Assign,
                        std::move(designator),
                        std::move(init)
                    );
                    elements.push_back(std::move(assign));
                }
            } else {
                // 普通初始化器
                auto init = parseInitializer();
                if (init) {
                    elements.push_back(std::move(init));
                }
            }
        } while (match(TokenKind::Comma) && !check(TokenKind::RightBrace));
    }

    expect(TokenKind::RightBrace, "需要 '}'");

    return std::make_unique<ast::InitListExpr>(loc, std::move(elements));
}

//=============================================================================
// 语句解析
//=============================================================================

ast::StmtPtr Parser::parseStatement() {
    // 标签语句
    if (check(TokenKind::Identifier)) {
        Token next = peek();
        if (next.kind == TokenKind::Colon) {
            return parseLabeledStatement();
        }
    }
    if (check(TokenKind::KW_case) || check(TokenKind::KW_default)) {
        return parseLabeledStatement();
    }

    // 复合语句
    if (check(TokenKind::LeftBrace)) {
        return parseCompoundStatement();
    }

    // 选择语句
    if (check(TokenKind::KW_if) || check(TokenKind::KW_switch)) {
        return parseSelectionStatement();
    }

    // 迭代语句
    if (check(TokenKind::KW_while) || check(TokenKind::KW_do) || check(TokenKind::KW_for)) {
        return parseIterationStatement();
    }

    // 跳转语句
    if (check(TokenKind::KW_goto) || check(TokenKind::KW_continue) ||
        check(TokenKind::KW_break) || check(TokenKind::KW_return)) {
        return parseJumpStatement();
    }

    // 表达式语句
    return parseExpressionStatement();
}

ast::StmtPtr Parser::parseLabeledStatement() {
    SourceLocation loc = current().location;

    if (match(TokenKind::KW_case)) {
        auto value = parseConditionalExpression();
        expect(TokenKind::Colon, "case 后需要 ':'");
        auto stmt = parseStatement();
        return std::make_unique<ast::CaseStmt>(loc, std::move(value), std::move(stmt));
    }

    if (match(TokenKind::KW_default)) {
        expect(TokenKind::Colon, "default 后需要 ':'");
        auto stmt = parseStatement();
        return std::make_unique<ast::DefaultStmt>(loc, std::move(stmt));
    }

    // 普通标签
    std::string label = std::string(current().text);
    advance();
    expect(TokenKind::Colon, "标签后需要 ':'");
    
    // C11 扩展：标签后面可以直接跟声明
    // 如果下一个是声明，则创建一个空语句来满足标签语法要求
    // 然后声明将在外层 parseCompoundStatement 中处理
    if (isDeclarationSpecifier()) {
        // 标签后跟声明，使用空语句
        auto emptyStmt = std::make_unique<ast::ExprStmt>(loc, nullptr);
        return std::make_unique<ast::LabelStmt>(loc, label, std::move(emptyStmt));
    }
    
    auto stmt = parseStatement();
    return std::make_unique<ast::LabelStmt>(loc, label, std::move(stmt));
}

std::unique_ptr<ast::CompoundStmt> Parser::parseCompoundStatement() {
    SourceLocation loc = current().location;
    expect(TokenKind::LeftBrace, "需要 '{'");

    auto compound = std::make_unique<ast::CompoundStmt>(loc);

    while (!check(TokenKind::RightBrace) && !isAtEnd()) {
        // 可能是声明或语句
        if (isDeclarationSpecifier()) {
            auto decls = parseDeclaration();
            for (auto& decl : decls) {
                if (decl) {
                    compound->items.push_back(std::move(decl));
                }
            }
        } else {
            auto stmt = parseStatement();
            if (stmt) {
                compound->items.push_back(std::move(stmt));
            }
        }
    }

    expect(TokenKind::RightBrace, "需要 '}'");
    return compound;
}

ast::StmtPtr Parser::parseSelectionStatement() {
    SourceLocation loc = current().location;

    if (match(TokenKind::KW_if)) {
        expect(TokenKind::LeftParen, "if 后需要 '('");
        auto cond = parseExpression();
        expect(TokenKind::RightParen, "条件后需要 ')'");
        auto thenStmt = parseStatement();

        ast::StmtPtr elseStmt;
        if (match(TokenKind::KW_else)) {
            elseStmt = parseStatement();
        }

        return std::make_unique<ast::IfStmt>(loc, std::move(cond),
            std::move(thenStmt), std::move(elseStmt));
    }

    if (match(TokenKind::KW_switch)) {
        expect(TokenKind::LeftParen, "switch 后需要 '('");
        auto cond = parseExpression();
        expect(TokenKind::RightParen, "条件后需要 ')'");
        auto body = parseStatement();

        return std::make_unique<ast::SwitchStmt>(loc, std::move(cond), std::move(body));
    }

    error("预期 if 或 switch");
    return nullptr;
}

ast::StmtPtr Parser::parseIterationStatement() {
    SourceLocation loc = current().location;

    if (match(TokenKind::KW_while)) {
        expect(TokenKind::LeftParen, "while 后需要 '('");
        auto cond = parseExpression();
        expect(TokenKind::RightParen, "条件后需要 ')'");
        auto body = parseStatement();

        return std::make_unique<ast::WhileStmt>(loc, std::move(cond), std::move(body));
    }

    if (match(TokenKind::KW_do)) {
        auto body = parseStatement();
        expect(TokenKind::KW_while, "do 体后需要 'while'");
        expect(TokenKind::LeftParen, "while 后需要 '('");
        auto cond = parseExpression();
        expect(TokenKind::RightParen, "条件后需要 ')'");
        expect(TokenKind::Semicolon, "do-while 后需要 ';'");

        return std::make_unique<ast::DoWhileStmt>(loc, std::move(body), std::move(cond));
    }

    if (match(TokenKind::KW_for)) {
        expect(TokenKind::LeftParen, "for 后需要 '('");

        auto forStmt = std::make_unique<ast::ForStmt>(loc);

        // 初始化部分
        if (!check(TokenKind::Semicolon)) {
            if (isDeclarationSpecifier()) {
                auto decls = parseDeclaration();
                forStmt->init = std::move(decls);
            } else {
                auto expr = parseExpression();
                forStmt->init = std::make_unique<ast::ExprStmt>(loc, std::move(expr));
                expect(TokenKind::Semicolon, "for 初始化后需要 ';'");
            }
        } else {
            forStmt->init = std::make_unique<ast::ExprStmt>(loc);
            advance();
        }

        // 条件部分
        if (!check(TokenKind::Semicolon)) {
            forStmt->condition = parseExpression();
        }
        expect(TokenKind::Semicolon, "for 条件后需要 ';'");

        // 增量部分
        if (!check(TokenKind::RightParen)) {
            forStmt->increment = parseExpression();
        }
        expect(TokenKind::RightParen, "for 头后需要 ')'");

        forStmt->body = parseStatement();
        return forStmt;
    }

    error("预期循环语句");
    return nullptr;
}

ast::StmtPtr Parser::parseJumpStatement() {
    SourceLocation loc = current().location;

    if (match(TokenKind::KW_goto)) {
        std::string label = std::string(current().text);
        expect(TokenKind::Identifier, "goto 后需要标签名");
        expect(TokenKind::Semicolon, "goto 后需要 ';'");
        return std::make_unique<ast::GotoStmt>(loc, label);
    }

    if (match(TokenKind::KW_continue)) {
        expect(TokenKind::Semicolon, "continue 后需要 ';'");
        return std::make_unique<ast::ContinueStmt>(loc);
    }

    if (match(TokenKind::KW_break)) {
        expect(TokenKind::Semicolon, "break 后需要 ';'");
        return std::make_unique<ast::BreakStmt>(loc);
    }

    if (match(TokenKind::KW_return)) {
        ast::ExprPtr value;
        if (!check(TokenKind::Semicolon)) {
            value = parseExpression();
        }
        expect(TokenKind::Semicolon, "return 后需要 ';'");
        return std::make_unique<ast::ReturnStmt>(loc, std::move(value));
    }

    error("预期跳转语句");
    return nullptr;
}

ast::StmtPtr Parser::parseExpressionStatement() {
    SourceLocation loc = current().location;

    if (match(TokenKind::Semicolon)) {
        return std::make_unique<ast::ExprStmt>(loc);  // 空语句
    }

    auto expr = parseExpression();
    expect(TokenKind::Semicolon, "语句后需要 ';'");
    return std::make_unique<ast::ExprStmt>(loc, std::move(expr));
}

//=============================================================================
// 表达式解析
//=============================================================================

ast::ExprPtr Parser::parseExpression() {
    auto expr = parseAssignmentExpression();

    while (match(TokenKind::Comma)) {
        SourceLocation loc = current().location;
        auto right = parseAssignmentExpression();
        expr = std::make_unique<ast::BinaryExpr>(loc, ast::BinaryOp::Comma,
            std::move(expr), std::move(right));
    }

    return expr;
}

ast::ExprPtr Parser::parseAssignmentExpression() {
    auto left = parseConditionalExpression();

    if (isAssignmentOperator(current().kind)) {
        SourceLocation loc = current().location;
        ast::BinaryOp op = assignOpToBinaryOp(current().kind);
        advance();
        auto right = parseAssignmentExpression();
        return std::make_unique<ast::BinaryExpr>(loc, op, std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseConditionalExpression() {
    auto cond = parseLogicalOrExpression();

    if (match(TokenKind::Question)) {
        SourceLocation loc = current().location;
        auto thenExpr = parseExpression();
        expect(TokenKind::Colon, "条件表达式需要 ':'");
        auto elseExpr = parseConditionalExpression();
        return std::make_unique<ast::ConditionalExpr>(loc, std::move(cond),
            std::move(thenExpr), std::move(elseExpr));
    }

    return cond;
}

ast::ExprPtr Parser::parseLogicalOrExpression() {
    auto left = parseLogicalAndExpression();

    while (match(TokenKind::PipePipe)) {
        SourceLocation loc = current().location;
        auto right = parseLogicalAndExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, ast::BinaryOp::LogOr,
            std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseLogicalAndExpression() {
    auto left = parseInclusiveOrExpression();

    while (match(TokenKind::AmpAmp)) {
        SourceLocation loc = current().location;
        auto right = parseInclusiveOrExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, ast::BinaryOp::LogAnd,
            std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseInclusiveOrExpression() {
    auto left = parseExclusiveOrExpression();

    while (match(TokenKind::Pipe)) {
        SourceLocation loc = current().location;
        auto right = parseExclusiveOrExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, ast::BinaryOp::BitOr,
            std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseExclusiveOrExpression() {
    auto left = parseAndExpression();

    while (match(TokenKind::Caret)) {
        SourceLocation loc = current().location;
        auto right = parseAndExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, ast::BinaryOp::BitXor,
            std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseAndExpression() {
    auto left = parseEqualityExpression();

    while (match(TokenKind::Amp)) {
        SourceLocation loc = current().location;
        auto right = parseEqualityExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, ast::BinaryOp::BitAnd,
            std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseEqualityExpression() {
    auto left = parseRelationalExpression();

    while (check(TokenKind::EqualEqual) || check(TokenKind::ExclaimEqual)) {
        SourceLocation loc = current().location;
        ast::BinaryOp op = current().kind == TokenKind::EqualEqual
                          ? ast::BinaryOp::Eq : ast::BinaryOp::Ne;
        advance();
        auto right = parseRelationalExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, op, std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseRelationalExpression() {
    auto left = parseShiftExpression();

    while (check(TokenKind::Less) || check(TokenKind::Greater) ||
           check(TokenKind::LessEqual) || check(TokenKind::GreaterEqual)) {
        SourceLocation loc = current().location;
        ast::BinaryOp op;
        switch (current().kind) {
            case TokenKind::Less: op = ast::BinaryOp::Lt; break;
            case TokenKind::Greater: op = ast::BinaryOp::Gt; break;
            case TokenKind::LessEqual: op = ast::BinaryOp::Le; break;
            case TokenKind::GreaterEqual: op = ast::BinaryOp::Ge; break;
            default: op = ast::BinaryOp::Lt;
        }
        advance();
        auto right = parseShiftExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, op, std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseShiftExpression() {
    auto left = parseAdditiveExpression();

    while (check(TokenKind::LessLess) || check(TokenKind::GreaterGreater)) {
        SourceLocation loc = current().location;
        ast::BinaryOp op = current().kind == TokenKind::LessLess
                          ? ast::BinaryOp::Shl : ast::BinaryOp::Shr;
        advance();
        auto right = parseAdditiveExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, op, std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseAdditiveExpression() {
    auto left = parseMultiplicativeExpression();

    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        SourceLocation loc = current().location;
        ast::BinaryOp op = current().kind == TokenKind::Plus
                          ? ast::BinaryOp::Add : ast::BinaryOp::Sub;
        advance();
        auto right = parseMultiplicativeExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, op, std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseMultiplicativeExpression() {
    auto left = parseCastExpression();

    while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
        SourceLocation loc = current().location;
        ast::BinaryOp op;
        switch (current().kind) {
            case TokenKind::Star: op = ast::BinaryOp::Mul; break;
            case TokenKind::Slash: op = ast::BinaryOp::Div; break;
            case TokenKind::Percent: op = ast::BinaryOp::Mod; break;
            default: op = ast::BinaryOp::Mul;
        }
        advance();
        auto right = parseCastExpression();
        left = std::make_unique<ast::BinaryExpr>(loc, op, std::move(left), std::move(right));
    }

    return left;
}

ast::ExprPtr Parser::parseCastExpression() {
    // 检查是否为类型转换
    if (check(TokenKind::LeftParen)) {
        // 需要向前看以确定是类型转换还是括号表达式
        Token next = peek();
        // 检查 next 是否可能是类型名的开始（包括类型限定符）
        if (next.kind == TokenKind::KW_void ||
            next.kind == TokenKind::KW_char || next.kind == TokenKind::KW_short ||
            next.kind == TokenKind::KW_int || next.kind == TokenKind::KW_long ||
            next.kind == TokenKind::KW_float || next.kind == TokenKind::KW_double ||
            next.kind == TokenKind::KW_signed || next.kind == TokenKind::KW_unsigned ||
            next.kind == TokenKind::KW_struct || next.kind == TokenKind::KW_union ||
            next.kind == TokenKind::KW_enum ||
            next.kind == TokenKind::KW_const || next.kind == TokenKind::KW_volatile ||
            next.kind == TokenKind::KW_restrict ||
            (next.kind == TokenKind::Identifier && typedefNames_.count(std::string(next.text)))) {
            // 可能是类型转换
            SourceLocation loc = current().location;
            advance();  // 跳过 (

            // 保存状态以便回退（简化版本不实现完整回退）
            if (isTypeName() || isTypeSpecifier()) {
                auto type = parseTypeName();
                if (match(TokenKind::RightParen)) {
                    auto operand = parseCastExpression();
                    return std::make_unique<ast::CastExpr>(loc, std::move(type), std::move(operand));
                }
            }
            // 不是类型转换，作为括号表达式处理
            auto expr = parseExpression();
            expect(TokenKind::RightParen, "需要 ')'");
            return expr;
        }
    }

    return parseUnaryExpression();
}

ast::ExprPtr Parser::parseUnaryExpression() {
    SourceLocation loc = current().location;

    // 前缀 ++/--
    if (match(TokenKind::PlusPlus)) {
        auto operand = parseUnaryExpression();
        return std::make_unique<ast::UnaryExpr>(loc, ast::UnaryOp::PreInc, std::move(operand));
    }
    if (match(TokenKind::MinusMinus)) {
        auto operand = parseUnaryExpression();
        return std::make_unique<ast::UnaryExpr>(loc, ast::UnaryOp::PreDec, std::move(operand));
    }

    // 一元运算符
    if (check(TokenKind::Amp) || check(TokenKind::Star) ||
        check(TokenKind::Plus) || check(TokenKind::Minus) ||
        check(TokenKind::Tilde) || check(TokenKind::Exclaim)) {

        ast::UnaryOp op;
        switch (current().kind) {
            case TokenKind::Amp: op = ast::UnaryOp::AddrOf; break;
            case TokenKind::Star: op = ast::UnaryOp::Deref; break;
            case TokenKind::Plus: op = ast::UnaryOp::Plus; break;
            case TokenKind::Minus: op = ast::UnaryOp::Minus; break;
            case TokenKind::Tilde: op = ast::UnaryOp::BitNot; break;
            case TokenKind::Exclaim: op = ast::UnaryOp::Not; break;
            default: op = ast::UnaryOp::Plus;
        }
        advance();
        auto operand = parseCastExpression();
        return std::make_unique<ast::UnaryExpr>(loc, op, std::move(operand));
    }

    // sizeof
    if (match(TokenKind::KW_sizeof)) {
        if (match(TokenKind::LeftParen)) {
            // sizeof(type) 或 sizeof(expr)
            if (isTypeName() || isTypeSpecifier()) {
                auto type = parseTypeName();
                expect(TokenKind::RightParen, "sizeof 后需要 ')'");
                return std::make_unique<ast::SizeofTypeExpr>(loc, std::move(type));
            }
            // sizeof(expr)
            auto expr = parseExpression();
            expect(TokenKind::RightParen, "sizeof 后需要 ')'");
            return std::make_unique<ast::UnaryExpr>(loc, ast::UnaryOp::Sizeof, std::move(expr));
        }
        // sizeof expr
        auto operand = parseUnaryExpression();
        return std::make_unique<ast::UnaryExpr>(loc, ast::UnaryOp::Sizeof, std::move(operand));
    }

    return parsePostfixExpression();
}

ast::ExprPtr Parser::parsePostfixExpression() {
    auto expr = parsePrimaryExpression();

    while (true) {
        SourceLocation loc = current().location;

        if (match(TokenKind::LeftBracket)) {
            // 数组下标
            auto index = parseExpression();
            expect(TokenKind::RightBracket, "需要 ']'");
            expr = std::make_unique<ast::SubscriptExpr>(loc, std::move(expr), std::move(index));
        } else if (match(TokenKind::LeftParen)) {
            // 函数调用
            auto args = parseArgumentExpressionList();
            expect(TokenKind::RightParen, "需要 ')'");
            expr = std::make_unique<ast::CallExpr>(loc, std::move(expr), std::move(args));
        } else if (match(TokenKind::Dot)) {
            // 成员访问
            std::string member = std::string(current().text);
            expect(TokenKind::Identifier, "需要成员名");
            expr = std::make_unique<ast::MemberExpr>(loc, std::move(expr), member, false);
        } else if (match(TokenKind::Arrow)) {
            // 指针成员访问
            std::string member = std::string(current().text);
            expect(TokenKind::Identifier, "需要成员名");
            expr = std::make_unique<ast::MemberExpr>(loc, std::move(expr), member, true);
        } else if (match(TokenKind::PlusPlus)) {
            // 后缀 ++
            expr = std::make_unique<ast::UnaryExpr>(loc, ast::UnaryOp::PostInc, std::move(expr));
        } else if (match(TokenKind::MinusMinus)) {
            // 后缀 --
            expr = std::make_unique<ast::UnaryExpr>(loc, ast::UnaryOp::PostDec, std::move(expr));
        } else {
            break;
        }
    }

    return expr;
}

ast::ExprPtr Parser::parsePrimaryExpression() {
    SourceLocation loc = current().location;

    // 标识符
    if (check(TokenKind::Identifier)) {
        std::string name = std::string(current().text);
        advance();
        return std::make_unique<ast::IdentExpr>(loc, name);
    }

    // 整数字面量
    if (check(TokenKind::IntLiteral)) {
        int64_t value = current().intValue;
        advance();
        return std::make_unique<ast::IntLiteral>(loc, value);
    }

    // 浮点字面量
    if (check(TokenKind::FloatLiteral)) {
        double value = current().floatValue;
        advance();
        return std::make_unique<ast::FloatLiteral>(loc, value);
    }

    // 字符字面量
    if (check(TokenKind::CharLiteral)) {
        char value = current().charValue;
        advance();
        return std::make_unique<ast::CharLiteral>(loc, value);
    }

    // 字符串字面量
    if (check(TokenKind::StringLiteral)) {
        std::string value = current().stringValue;
        advance();

        // 连接相邻字符串
        while (check(TokenKind::StringLiteral)) {
            value += current().stringValue;
            advance();
        }

        return std::make_unique<ast::StringLiteral>(loc, value);
    }

    // 括号表达式
    if (match(TokenKind::LeftParen)) {
        auto expr = parseExpression();
        expect(TokenKind::RightParen, "需要 ')'");
        return expr;
    }

    error("预期表达式");
    return nullptr;
}

ast::ExprList Parser::parseArgumentExpressionList() {
    ast::ExprList args;

    if (check(TokenKind::RightParen)) {
        return args;  // 空参数列表
    }

    do {
        auto arg = parseAssignmentExpression();
        if (arg) {
            args.push_back(std::move(arg));
        }
    } while (match(TokenKind::Comma));

    return args;
}

//=============================================================================
// 运算符辅助
//=============================================================================

bool Parser::isAssignmentOperator(TokenKind kind) {
    switch (kind) {
        case TokenKind::Equal:
        case TokenKind::PlusEqual:
        case TokenKind::MinusEqual:
        case TokenKind::StarEqual:
        case TokenKind::SlashEqual:
        case TokenKind::PercentEqual:
        case TokenKind::AmpEqual:
        case TokenKind::PipeEqual:
        case TokenKind::CaretEqual:
        case TokenKind::LessLessEqual:
        case TokenKind::GreaterGreaterEqual:
            return true;
        default:
            return false;
    }
}

ast::BinaryOp Parser::assignOpToBinaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Equal: return ast::BinaryOp::Assign;
        case TokenKind::PlusEqual: return ast::BinaryOp::AddAssign;
        case TokenKind::MinusEqual: return ast::BinaryOp::SubAssign;
        case TokenKind::StarEqual: return ast::BinaryOp::MulAssign;
        case TokenKind::SlashEqual: return ast::BinaryOp::DivAssign;
        case TokenKind::PercentEqual: return ast::BinaryOp::ModAssign;
        case TokenKind::AmpEqual: return ast::BinaryOp::AndAssign;
        case TokenKind::PipeEqual: return ast::BinaryOp::OrAssign;
        case TokenKind::CaretEqual: return ast::BinaryOp::XorAssign;
        case TokenKind::LessLessEqual: return ast::BinaryOp::ShlAssign;
        case TokenKind::GreaterGreaterEqual: return ast::BinaryOp::ShrAssign;
        default: return ast::BinaryOp::Assign;
    }
}

ast::BinaryOp Parser::tokenToBinaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus: return ast::BinaryOp::Add;
        case TokenKind::Minus: return ast::BinaryOp::Sub;
        case TokenKind::Star: return ast::BinaryOp::Mul;
        case TokenKind::Slash: return ast::BinaryOp::Div;
        case TokenKind::Percent: return ast::BinaryOp::Mod;
        case TokenKind::Amp: return ast::BinaryOp::BitAnd;
        case TokenKind::Pipe: return ast::BinaryOp::BitOr;
        case TokenKind::Caret: return ast::BinaryOp::BitXor;
        case TokenKind::LessLess: return ast::BinaryOp::Shl;
        case TokenKind::GreaterGreater: return ast::BinaryOp::Shr;
        case TokenKind::Less: return ast::BinaryOp::Lt;
        case TokenKind::Greater: return ast::BinaryOp::Gt;
        case TokenKind::LessEqual: return ast::BinaryOp::Le;
        case TokenKind::GreaterEqual: return ast::BinaryOp::Ge;
        case TokenKind::EqualEqual: return ast::BinaryOp::Eq;
        case TokenKind::ExclaimEqual: return ast::BinaryOp::Ne;
        case TokenKind::AmpAmp: return ast::BinaryOp::LogAnd;
        case TokenKind::PipePipe: return ast::BinaryOp::LogOr;
        case TokenKind::Comma: return ast::BinaryOp::Comma;
        default: return ast::BinaryOp::Add;
    }
}

ast::UnaryOp Parser::tokenToUnaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus: return ast::UnaryOp::Plus;
        case TokenKind::Minus: return ast::UnaryOp::Minus;
        case TokenKind::Exclaim: return ast::UnaryOp::Not;
        case TokenKind::Tilde: return ast::UnaryOp::BitNot;
        case TokenKind::Star: return ast::UnaryOp::Deref;
        case TokenKind::Amp: return ast::UnaryOp::AddrOf;
        case TokenKind::PlusPlus: return ast::UnaryOp::PreInc;
        case TokenKind::MinusMinus: return ast::UnaryOp::PreDec;
        default: return ast::UnaryOp::Plus;
    }
}

} // namespace cdd
