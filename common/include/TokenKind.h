/**
 * @file TokenKind.h
 * @brief 词法单元类型定义
 * @author mozihe
 * 
 * 定义完整的 C 语言 Token 类型枚举及相关操作函数。
 * Token 类型按类别组织：特殊 Token、字面量、关键字、标点符号、运算符。
 */

#ifndef CDD_TOKEN_KIND_H
#define CDD_TOKEN_KIND_H

#include <cstdint>
#include <string_view>

namespace cdd {

/**
 * @enum TokenKind
 * @brief Token 类型枚举
 *
 * 按类别组织：
 * - 特殊 Token (0-9)
 * - 字面量 (10-19)
 * - 关键字 (20-59)
 * - 标点符号 (60-79)
 * - 运算符 (80-149)
 */
enum class TokenKind : uint8_t {
    // ========== 特殊 Token ==========
    Invalid = 0,        ///< 无效 Token
    EndOfFile,          ///< 文件结束
    Comment,            ///< 注释（被跳过）

    // ========== 字面量 ==========
    Identifier = 10,    ///< 标识符
    IntLiteral,         ///< 整数字面量
    FloatLiteral,       ///< 浮点数字面量
    CharLiteral,        ///< 字符字面量
    StringLiteral,      ///< 字符串字面量

    // ========== 关键字 - 类型说明符 ==========
    KW_void = 20,
    KW_char,
    KW_short,
    KW_int,
    KW_long,
    KW_float,
    KW_double,
    KW_signed,
    KW_unsigned,

    // ========== 关键字 - 类型限定符 ==========
    KW_const = 30,
    KW_volatile,
    KW_restrict,        // C99

    // ========== 关键字 - 存储类说明符 ==========
    KW_auto = 35,
    KW_register,
    KW_static,
    KW_extern,
    KW_typedef,

    // ========== 关键字 - 结构类型 ==========
    KW_struct = 40,
    KW_union,
    KW_enum,

    // ========== 关键字 - 控制流 ==========
    KW_if = 45,
    KW_else,
    KW_switch,
    KW_case,
    KW_default,
    KW_while,
    KW_do,
    KW_for,
    KW_break,
    KW_continue,
    KW_goto,
    KW_return,

    // ========== 关键字 - 其他 ==========
    KW_sizeof = 58,
    KW_inline,          // C99

    // ========== 标点符号 ==========
    LeftParen = 60,     ///< (
    RightParen,         ///< )
    LeftBracket,        ///< [
    RightBracket,       ///< ]
    LeftBrace,          ///< {
    RightBrace,         ///< }
    Semicolon,          ///< ;
    Comma,              ///< ,
    Colon,              ///< :
    Ellipsis,           ///< ...

    // ========== 运算符 - 算术 ==========
    Plus = 80,          ///< +
    Minus,              ///< -
    Star,               ///< *
    Slash,              ///< /
    Percent,            ///< %

    // ========== 运算符 - 位运算 ==========
    Amp = 90,           ///< &
    Pipe,               ///< |
    Caret,              ///< ^
    Tilde,              ///< ~
    LessLess,           ///< <<
    GreaterGreater,     ///< >>

    // ========== 运算符 - 逻辑 ==========
    Exclaim = 100,      ///< !
    AmpAmp,             ///< &&
    PipePipe,           ///< ||

    // ========== 运算符 - 比较 ==========
    EqualEqual = 105,   ///< ==
    ExclaimEqual,       ///< !=
    Less,               ///< <
    Greater,            ///< >
    LessEqual,          ///< <=
    GreaterEqual,       ///< >=

    // ========== 运算符 - 赋值 ==========
    Equal = 115,        ///< =
    PlusEqual,          ///< +=
    MinusEqual,         ///< -=
    StarEqual,          ///< *=
    SlashEqual,         ///< /=
    PercentEqual,       ///< %=
    AmpEqual,           ///< &=
    PipeEqual,          ///< |=
    CaretEqual,         ///< ^=
    LessLessEqual,      ///< <<=
    GreaterGreaterEqual,///< >>=

    // ========== 运算符 - 自增自减 ==========
    PlusPlus = 130,     ///< ++
    MinusMinus,         ///< --

    // ========== 运算符 - 成员访问 ==========
    Dot = 135,          ///< .
    Arrow,              ///< ->

    // ========== 运算符 - 其他 ==========
    Question = 140,     ///< ?

    // 最大值，用于数组大小
    NUM_TOKENS
};

/**
 * @brief 获取 TokenKind 的字符串名称
 */
constexpr const char* tokenKindName(TokenKind kind) {
    switch (kind) {
        case TokenKind::Invalid:        return "Invalid";
        case TokenKind::EndOfFile:      return "EndOfFile";
        case TokenKind::Comment:        return "Comment";
        case TokenKind::Identifier:     return "Identifier";
        case TokenKind::IntLiteral:     return "IntLiteral";
        case TokenKind::FloatLiteral:   return "FloatLiteral";
        case TokenKind::CharLiteral:    return "CharLiteral";
        case TokenKind::StringLiteral:  return "StringLiteral";

        // 关键字
        case TokenKind::KW_void:        return "void";
        case TokenKind::KW_char:        return "char";
        case TokenKind::KW_short:       return "short";
        case TokenKind::KW_int:         return "int";
        case TokenKind::KW_long:        return "long";
        case TokenKind::KW_float:       return "float";
        case TokenKind::KW_double:      return "double";
        case TokenKind::KW_signed:      return "signed";
        case TokenKind::KW_unsigned:    return "unsigned";
        case TokenKind::KW_const:       return "const";
        case TokenKind::KW_volatile:    return "volatile";
        case TokenKind::KW_restrict:    return "restrict";
        case TokenKind::KW_auto:        return "auto";
        case TokenKind::KW_register:    return "register";
        case TokenKind::KW_static:      return "static";
        case TokenKind::KW_extern:      return "extern";
        case TokenKind::KW_typedef:     return "typedef";
        case TokenKind::KW_struct:      return "struct";
        case TokenKind::KW_union:       return "union";
        case TokenKind::KW_enum:        return "enum";
        case TokenKind::KW_if:          return "if";
        case TokenKind::KW_else:        return "else";
        case TokenKind::KW_switch:      return "switch";
        case TokenKind::KW_case:        return "case";
        case TokenKind::KW_default:     return "default";
        case TokenKind::KW_while:       return "while";
        case TokenKind::KW_do:          return "do";
        case TokenKind::KW_for:         return "for";
        case TokenKind::KW_break:       return "break";
        case TokenKind::KW_continue:    return "continue";
        case TokenKind::KW_goto:        return "goto";
        case TokenKind::KW_return:      return "return";
        case TokenKind::KW_sizeof:      return "sizeof";
        case TokenKind::KW_inline:      return "inline";

        // 标点符号
        case TokenKind::LeftParen:      return "(";
        case TokenKind::RightParen:     return ")";
        case TokenKind::LeftBracket:    return "[";
        case TokenKind::RightBracket:   return "]";
        case TokenKind::LeftBrace:      return "{";
        case TokenKind::RightBrace:     return "}";
        case TokenKind::Semicolon:      return ";";
        case TokenKind::Comma:          return ",";
        case TokenKind::Colon:          return ":";
        case TokenKind::Ellipsis:       return "...";

        // 运算符
        case TokenKind::Plus:           return "+";
        case TokenKind::Minus:          return "-";
        case TokenKind::Star:           return "*";
        case TokenKind::Slash:          return "/";
        case TokenKind::Percent:        return "%";
        case TokenKind::Amp:            return "&";
        case TokenKind::Pipe:           return "|";
        case TokenKind::Caret:          return "^";
        case TokenKind::Tilde:          return "~";
        case TokenKind::LessLess:       return "<<";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::Exclaim:        return "!";
        case TokenKind::AmpAmp:         return "&&";
        case TokenKind::PipePipe:       return "||";
        case TokenKind::EqualEqual:     return "==";
        case TokenKind::ExclaimEqual:   return "!=";
        case TokenKind::Less:           return "<";
        case TokenKind::Greater:        return ">";
        case TokenKind::LessEqual:      return "<=";
        case TokenKind::GreaterEqual:   return ">=";
        case TokenKind::Equal:          return "=";
        case TokenKind::PlusEqual:      return "+=";
        case TokenKind::MinusEqual:     return "-=";
        case TokenKind::StarEqual:      return "*=";
        case TokenKind::SlashEqual:     return "/=";
        case TokenKind::PercentEqual:   return "%=";
        case TokenKind::AmpEqual:       return "&=";
        case TokenKind::PipeEqual:      return "|=";
        case TokenKind::CaretEqual:     return "^=";
        case TokenKind::LessLessEqual:  return "<<=";
        case TokenKind::GreaterGreaterEqual: return ">>=";
        case TokenKind::PlusPlus:       return "++";
        case TokenKind::MinusMinus:     return "--";
        case TokenKind::Dot:            return ".";
        case TokenKind::Arrow:          return "->";
        case TokenKind::Question:       return "?";

        default:                        return "???";
    }
}

/**
 * @brief 获取 TokenKind 对应的标点符号（如果有）
 */
constexpr const char* tokenKindPunctuation(TokenKind kind) {
    switch (kind) {
        case TokenKind::LeftParen:      return "(";
        case TokenKind::RightParen:     return ")";
        case TokenKind::LeftBracket:    return "[";
        case TokenKind::RightBracket:   return "]";
        case TokenKind::LeftBrace:      return "{";
        case TokenKind::RightBrace:     return "}";
        case TokenKind::Semicolon:      return ";";
        case TokenKind::Comma:          return ",";
        case TokenKind::Colon:          return ":";
        case TokenKind::Ellipsis:       return "...";
        case TokenKind::Plus:           return "+";
        case TokenKind::Minus:          return "-";
        case TokenKind::Star:           return "*";
        case TokenKind::Slash:          return "/";
        case TokenKind::Percent:        return "%";
        case TokenKind::Amp:            return "&";
        case TokenKind::Pipe:           return "|";
        case TokenKind::Caret:          return "^";
        case TokenKind::Tilde:          return "~";
        case TokenKind::LessLess:       return "<<";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::Exclaim:        return "!";
        case TokenKind::AmpAmp:         return "&&";
        case TokenKind::PipePipe:       return "||";
        case TokenKind::EqualEqual:     return "==";
        case TokenKind::ExclaimEqual:   return "!=";
        case TokenKind::Less:           return "<";
        case TokenKind::Greater:        return ">";
        case TokenKind::LessEqual:      return "<=";
        case TokenKind::GreaterEqual:   return ">=";
        case TokenKind::Equal:          return "=";
        case TokenKind::PlusEqual:      return "+=";
        case TokenKind::MinusEqual:     return "-=";
        case TokenKind::StarEqual:      return "*=";
        case TokenKind::SlashEqual:     return "/=";
        case TokenKind::PercentEqual:   return "%=";
        case TokenKind::AmpEqual:       return "&=";
        case TokenKind::PipeEqual:      return "|=";
        case TokenKind::CaretEqual:     return "^=";
        case TokenKind::LessLessEqual:  return "<<=";
        case TokenKind::GreaterGreaterEqual: return ">>=";
        case TokenKind::PlusPlus:       return "++";
        case TokenKind::MinusMinus:     return "--";
        case TokenKind::Dot:            return ".";
        case TokenKind::Arrow:          return "->";
        case TokenKind::Question:       return "?";
        default:                        return nullptr;
    }
}

/**
 * @brief 判断是否为关键字
 */
constexpr bool isKeyword(TokenKind kind) {
    return kind >= TokenKind::KW_void && kind <= TokenKind::KW_inline;
}

/**
 * @brief 判断是否为类型说明符关键字
 */
constexpr bool isTypeSpecifier(TokenKind kind) {
    return kind >= TokenKind::KW_void && kind <= TokenKind::KW_unsigned;
}

/**
 * @brief 判断是否为类型限定符
 */
constexpr bool isTypeQualifier(TokenKind kind) {
    return kind >= TokenKind::KW_const && kind <= TokenKind::KW_restrict;
}

/**
 * @brief 判断是否为存储类说明符
 */
constexpr bool isStorageClass(TokenKind kind) {
    return kind >= TokenKind::KW_auto && kind <= TokenKind::KW_typedef;
}

/**
 * @brief 判断是否为字面量
 */
constexpr bool isLiteral(TokenKind kind) {
    return kind >= TokenKind::IntLiteral && kind <= TokenKind::StringLiteral;
}

/**
 * @brief 判断是否为赋值运算符
 */
constexpr bool isAssignmentOp(TokenKind kind) {
    return kind >= TokenKind::Equal && kind <= TokenKind::GreaterGreaterEqual;
}

/**
 * @brief 判断是否为一元运算符
 */
constexpr bool isUnaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Amp:
        case TokenKind::Exclaim:
        case TokenKind::Tilde:
        case TokenKind::PlusPlus:
        case TokenKind::MinusMinus:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 判断是否为二元运算符
 */
constexpr bool isBinaryOp(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus: case TokenKind::Minus:
        case TokenKind::Star: case TokenKind::Slash: case TokenKind::Percent:
        case TokenKind::Amp: case TokenKind::Pipe: case TokenKind::Caret:
        case TokenKind::LessLess: case TokenKind::GreaterGreater:
        case TokenKind::AmpAmp: case TokenKind::PipePipe:
        case TokenKind::EqualEqual: case TokenKind::ExclaimEqual:
        case TokenKind::Less: case TokenKind::Greater:
        case TokenKind::LessEqual: case TokenKind::GreaterEqual:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 获取二元运算符优先级（越高越先绑定）
 */
constexpr int getBinaryPrecedence(TokenKind kind) {
    switch (kind) {
        // 乘法类
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
            return 13;

        // 加法类
        case TokenKind::Plus:
        case TokenKind::Minus:
            return 12;

        // 移位
        case TokenKind::LessLess:
        case TokenKind::GreaterGreater:
            return 11;

        // 关系
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::LessEqual:
        case TokenKind::GreaterEqual:
            return 10;

        // 相等
        case TokenKind::EqualEqual:
        case TokenKind::ExclaimEqual:
            return 9;

        // 位与
        case TokenKind::Amp:
            return 8;

        // 位异或
        case TokenKind::Caret:
            return 7;

        // 位或
        case TokenKind::Pipe:
            return 6;

        // 逻辑与
        case TokenKind::AmpAmp:
            return 5;

        // 逻辑或
        case TokenKind::PipePipe:
            return 4;

        // 条件运算符 ? : 在这里不处理

        // 赋值 (右结合，在 parseAssignment 中特殊处理)
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
            return 2;

        // 逗号
        case TokenKind::Comma:
            return 1;

        default:
            return 0;  // 不是二元运算符
    }
}

/**
 * @brief 判断运算符是否右结合
 */
constexpr bool isRightAssociative(TokenKind kind) {
    // 赋值运算符和条件运算符是右结合的
    return isAssignmentOp(kind) || kind == TokenKind::Question;
}

/**
 * @brief 从关键字字符串查找 TokenKind
 */
inline TokenKind keywordToTokenKind(std::string_view str) {
    // 按长度分组优化查找
    switch (str.length()) {
        case 2:
            if (str == "if") return TokenKind::KW_if;
            if (str == "do") return TokenKind::KW_do;
            break;
        case 3:
            if (str == "int") return TokenKind::KW_int;
            if (str == "for") return TokenKind::KW_for;
            break;
        case 4:
            if (str == "void") return TokenKind::KW_void;
            if (str == "char") return TokenKind::KW_char;
            if (str == "long") return TokenKind::KW_long;
            if (str == "auto") return TokenKind::KW_auto;
            if (str == "else") return TokenKind::KW_else;
            if (str == "case") return TokenKind::KW_case;
            if (str == "goto") return TokenKind::KW_goto;
            if (str == "enum") return TokenKind::KW_enum;
            break;
        case 5:
            if (str == "short") return TokenKind::KW_short;
            if (str == "float") return TokenKind::KW_float;
            if (str == "const") return TokenKind::KW_const;
            if (str == "while") return TokenKind::KW_while;
            if (str == "break") return TokenKind::KW_break;
            if (str == "union") return TokenKind::KW_union;
            break;
        case 6:
            if (str == "double") return TokenKind::KW_double;
            if (str == "signed") return TokenKind::KW_signed;
            if (str == "extern") return TokenKind::KW_extern;
            if (str == "static") return TokenKind::KW_static;
            if (str == "switch") return TokenKind::KW_switch;
            if (str == "return") return TokenKind::KW_return;
            if (str == "sizeof") return TokenKind::KW_sizeof;
            if (str == "struct") return TokenKind::KW_struct;
            if (str == "inline") return TokenKind::KW_inline;
            break;
        case 7:
            if (str == "default") return TokenKind::KW_default;
            if (str == "typedef") return TokenKind::KW_typedef;
            break;
        case 8:
            if (str == "unsigned") return TokenKind::KW_unsigned;
            if (str == "volatile") return TokenKind::KW_volatile;
            if (str == "continue") return TokenKind::KW_continue;
            if (str == "register") return TokenKind::KW_register;
            if (str == "restrict") return TokenKind::KW_restrict;
            break;
    }
    return TokenKind::Invalid;  // 不是关键字
}

/**
 * @brief 从标点符号字符串查找 TokenKind
 */
inline TokenKind punctuatorToTokenKind(std::string_view str) {
    switch (str.length()) {
        case 1:
            switch (str[0]) {
                case '(': return TokenKind::LeftParen;
                case ')': return TokenKind::RightParen;
                case '[': return TokenKind::LeftBracket;
                case ']': return TokenKind::RightBracket;
                case '{': return TokenKind::LeftBrace;
                case '}': return TokenKind::RightBrace;
                case ';': return TokenKind::Semicolon;
                case ',': return TokenKind::Comma;
                case ':': return TokenKind::Colon;
                case '+': return TokenKind::Plus;
                case '-': return TokenKind::Minus;
                case '*': return TokenKind::Star;
                case '/': return TokenKind::Slash;
                case '%': return TokenKind::Percent;
                case '&': return TokenKind::Amp;
                case '|': return TokenKind::Pipe;
                case '^': return TokenKind::Caret;
                case '~': return TokenKind::Tilde;
                case '!': return TokenKind::Exclaim;
                case '<': return TokenKind::Less;
                case '>': return TokenKind::Greater;
                case '=': return TokenKind::Equal;
                case '.': return TokenKind::Dot;
                case '?': return TokenKind::Question;
            }
            break;
        case 2:
            if (str == "++") return TokenKind::PlusPlus;
            if (str == "--") return TokenKind::MinusMinus;
            if (str == "+=") return TokenKind::PlusEqual;
            if (str == "-=") return TokenKind::MinusEqual;
            if (str == "*=") return TokenKind::StarEqual;
            if (str == "/=") return TokenKind::SlashEqual;
            if (str == "%=") return TokenKind::PercentEqual;
            if (str == "&=") return TokenKind::AmpEqual;
            if (str == "|=") return TokenKind::PipeEqual;
            if (str == "^=") return TokenKind::CaretEqual;
            if (str == "&&") return TokenKind::AmpAmp;
            if (str == "||") return TokenKind::PipePipe;
            if (str == "==") return TokenKind::EqualEqual;
            if (str == "!=") return TokenKind::ExclaimEqual;
            if (str == "<=") return TokenKind::LessEqual;
            if (str == ">=") return TokenKind::GreaterEqual;
            if (str == "<<") return TokenKind::LessLess;
            if (str == ">>") return TokenKind::GreaterGreater;
            if (str == "->") return TokenKind::Arrow;
            break;
        case 3:
            if (str == "...") return TokenKind::Ellipsis;
            if (str == "<<=") return TokenKind::LessLessEqual;
            if (str == ">>=") return TokenKind::GreaterGreaterEqual;
            break;
    }
    return TokenKind::Invalid;
}

} // namespace cdd

#endif // CDD_TOKEN_KIND_H
