/**
 * @file Lexer.h
 * @brief 词法分析器
 * @author mozihe
 * 
 * 基于 DFA 的词法分析器，将源代码转换为 Token 流。
 * 支持 C 语言所有词法元素：标识符、关键字、数字、字符串、运算符等。
 */

#ifndef CDD_LEXER_H
#define CDD_LEXER_H

#include "TokenKind.h"
#include "SourceLocation.h"
#include "StateMachine.h"
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace cdd {

/**
 * @enum LexerState
 * @brief DFA 状态枚举
 * 
 * 词法分析器使用的所有状态，按类别分组。
 */
enum class LexerState {
    Start,              ///< 初始状态
    Done,               ///< 终止状态（Token 完成）
    Error,              ///< 错误状态
    
    InIdentifier,       ///< 读取标识符
    
    InInteger,          ///< 十进制整数
    InOctal,            ///< 八进制数
    InHex,              ///< 十六进制数
    InHexStart,         ///< 0x/0X 之后
    InZero,             ///< 前导 0
    InFloat,            ///< 浮点小数部分
    InFloatExp,         ///< 浮点指数部分
    InFloatExpSign,     ///< 浮点指数符号
    InFloatDot,         ///< 小数点之后
    
    InChar,             ///< 字符字面量
    InCharEscape,       ///< 字符转义
    InCharOctal1,       ///< 八进制转义第一位
    InCharOctal2,       ///< 八进制转义第二位
    InCharHexStart,     ///< \x 之后
    InCharHex1,         ///< 十六进制转义
    InCharEnd,          ///< 等待闭合引号
    InString,           ///< 字符串字面量
    InStringEscape,     ///< 字符串转义
    InStringOctal1,     ///< 字符串八进制转义第一位
    InStringOctal2,     ///< 字符串八进制转义第二位
    InStringHexStart,   ///< 字符串 \x 之后
    InStringHex1,       ///< 字符串十六进制转义
    
    InLineComment,      ///< 单行注释
    InBlockComment,     ///< 块注释
    InBlockCommentStar, ///< 块注释中遇到 *
    
    InSlash,            ///< / 之后
    InPlus,             ///< + 之后
    InMinus,            ///< - 之后
    InAmp,              ///< & 之后
    InPipe,             ///< | 之后
    InEqual,            ///< = 之后
    InExclaim,          ///< ! 之后
    InLess,             ///< < 之后
    InGreater,          ///< > 之后
    InStar,             ///< * 之后
    InPercent,          ///< % 之后
    InCaret,            ///< ^ 之后
    InDot,              ///< . 之后
    InDotDot,           ///< .. 之后
    InLessLess,         ///< << 之后
    InGreaterGreater,   ///< >> 之后
    
    InIntSuffixU,       ///< u/U 后缀
    InIntSuffixL,       ///< l/L 后缀
    InIntSuffixUL,      ///< ul/UL 后缀
    InIntSuffixLL,      ///< ll/LL 后缀
    
    InBinaryStart,      ///< 0b/0B 之后
    InBinary,           ///< 二进制数
};

/**
 * @struct Token
 * @brief 词法单元结构
 * 
 * 包含 Token 类型、源码位置和字面量值。
 */
struct Token {
    TokenKind kind;             ///< Token 类型
    SourceLocation location;    ///< 源码位置
    std::string_view text;      ///< 原始文本

    union {
        int64_t intValue;       ///< 整数字面量值
        double floatValue;      ///< 浮点字面量值
        char charValue;         ///< 字符字面量值
    };

    std::string stringValue;    ///< 字符串/标识符值

    Token() : kind(TokenKind::Invalid), intValue(0) {}
    explicit Token(TokenKind k) : kind(k), intValue(0) {}
    Token(TokenKind k, SourceLocation loc) : kind(k), location(loc), intValue(0) {}
    Token(TokenKind k, SourceLocation loc, std::string_view txt)
        : kind(k), location(loc), text(txt), intValue(0) {}

    /** @brief 检查是否为指定类型 */
    bool is(TokenKind k) const { return kind == k; }
    
    /** @brief 检查是否为多个类型之一 */
    template<typename... Kinds>
    bool isOneOf(Kinds... kinds) const { return ((kind == kinds) || ...); }
    
    /** @brief 是否为文件结束 */
    bool isEof() const { return kind == TokenKind::EndOfFile; }
    
    /** @brief 是否为关键字 */
    bool isKeyword() const { return cdd::isKeyword(kind); }
    
    /** @brief 是否为字面量 */
    bool isLiteral() const { return cdd::isLiteral(kind); }
    
    /** @brief 是否为标识符 */
    bool isIdentifier() const { return kind == TokenKind::Identifier; }
    
    /** @brief 获取 Token 名称 */
    std::string_view getName() const {
        if (kind == TokenKind::Identifier) return stringValue;
        return tokenKindName(kind);
    }
};

/**
 * @struct LexerError
 * @brief 词法分析错误信息
 */
struct LexerError {
    SourceLocation location;  ///< 错误位置
    std::string message;      ///< 错误消息
};

/**
 * @class Lexer
 * @brief 基于 DFA 的词法分析器
 * 
 * 将源代码字符流转换为 Token 流。使用状态机处理多字符 Token。
 */
class Lexer {
public:
    /**
     * @brief 构造词法分析器
     * @param source 源代码字符串
     * @param filename 文件名（用于错误报告）
     */
    explicit Lexer(std::string_view source, std::string_view filename = "<input>");

    /**
     * @brief 获取下一个 Token
     * @return 下一个 Token，文件结束返回 EndOfFile
     */
    Token nextToken();
    
    /**
     * @brief 预览下一个 Token（不消费）
     * @return 下一个 Token
     */
    Token peekToken();
    
    /**
     * @brief 获取当前源码位置
     * @return 当前位置
     */
    SourceLocation currentLocation() const;
    
    /**
     * @brief 是否已到文件末尾
     * @return 到达末尾返回 true
     */
    bool isEof() const { return pos_ >= source_.size(); }
    
    /**
     * @brief 获取源代码
     * @return 源代码字符串视图
     */
    std::string_view source() const { return source_; }
    
    /**
     * @brief 获取词法分析错误列表
     * @return 错误列表的常量引用
     */
    const std::vector<LexerError>& errors() const { return errors_; }
    
    /**
     * @brief 检查是否有词法分析错误
     * @return 有错误返回 true
     */
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::string_view source_;
    std::string_view filename_;
    size_t pos_ = 0;
    uint32_t line_ = 1;
    uint32_t column_ = 1;

    Token cachedToken_;
    bool hasCachedToken_ = false;

    using DFA = StateMachine<LexerState, char>;
    DFA dfa_;
    
    size_t tokenStart_ = 0;
    SourceLocation tokenLoc_;
    std::string lexeme_;
    TokenKind pendingKind_ = TokenKind::Invalid;
    
    std::vector<LexerError> errors_;  ///< 词法分析错误列表
    
    /**
     * @brief 报告词法错误
     * @param loc 错误位置
     * @param message 错误消息
     */
    void reportError(const SourceLocation& loc, const std::string& message);

    /**
     * @brief 初始化 DFA 状态机
     * 
     * 设置所有状态转移规则。
     */
    void initDFA();
    
    /**
     * @brief 获取回退状态
     * @param s 当前状态
     * @return 回退后应产生的状态（Done 或 Error）
     */
    LexerState getFallbackState(LexerState s) const;
    
    /**
     * @brief 根据状态完成 Token
     * @param state 当前状态
     * @return 完成的 Token
     */
    Token finalizeToken(LexerState state);
    
    /**
     * @brief 完成运算符 Token
     * @return 运算符 Token
     */
    Token finalizeOperator();

    /**
     * @brief 预览字符
     * @param offset 偏移量（默认 0 表示当前字符）
     * @return 指定位置的字符，超出范围返回 '\0'
     */
    char peek(size_t offset = 0) const;
    
    /**
     * @brief 消费并返回当前字符
     * @return 当前字符
     */
    char advance();
    
    /**
     * @brief 跳过空白字符
     */
    void skipWhitespace();

    /**
     * @brief DFA 主循环，扫描下一个 Token
     * @return 扫描到的 Token
     */
    Token scanToken();

    /** @brief 判断是否为十进制数字 */
    bool isDigit(char c) const { return c >= '0' && c <= '9'; }
    
    /** @brief 判断是否为十六进制数字 */
    bool isHexDigit(char c) const {
        return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }
    
    /** @brief 判断是否为八进制数字 */
    bool isOctalDigit(char c) const { return c >= '0' && c <= '7'; }
    
    /** @brief 判断是否为字母或下划线 */
    bool isAlpha(char c) const {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }
    
    /** @brief 判断是否为字母、数字或下划线 */
    bool isAlphaNumeric(char c) const { return isAlpha(c) || isDigit(c); }
    
    /**
     * @brief 处理转义字符
     * @param c 转义字符（\ 后的字符）
     * @return 转义后的实际字符
     */
    char processEscape(char c) const;
    
    /**
     * @brief 解析字符字面量
     * @param s 包含引号的字符字面量字符串
     * @return 字符值
     */
    char parseCharLiteral(const std::string& s) const;
    
    /**
     * @brief 处理字符串中的转义序列
     * @param s 原始字符串（不含引号）
     * @return 处理转义后的字符串
     */
    std::string processStringEscapes(const std::string& s) const;
    
    /**
     * @brief 解析整数字面量
     * @param s 数字字符串
     * @param base 进制（8/10/16）
     * @return 整数值
     */
    int64_t parseInteger(const std::string& s, int base) const;
    
    /**
     * @brief 解析浮点字面量
     * @param s 数字字符串
     * @return 浮点值
     */
    double parseFloat(const std::string& s) const;
};

} // namespace cdd

#endif // CDD_LEXER_H
