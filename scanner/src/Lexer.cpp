/**
 * @file Lexer.cpp
 * @brief 词法分析器实现
 * @author mozihe
 */

#include "Lexer.h"
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>

namespace cdd {

Lexer::Lexer(std::string_view source, std::string_view filename)
    : source_(source), filename_(filename) {
    initDFA();
}

/**
 * @brief 初始化 DFA 状态转移表
 * 
 * 构建词法分析状态机的所有转移规则，按类别分组：
 * - 标识符：[a-zA-Z_] 开头
 * - 数字：整数、浮点、十六进制、八进制、二进制
 * - 字符/字符串：单引号和双引号
 * - 注释：// 和 /*
 * - 运算符：单字符和多字符运算符
 */
void Lexer::initDFA() {
    dfa_.setInitialState(LexerState::Start);
    using S = LexerState;

    // 辅助 lambda：添加字符范围的转移
    auto addRange = [&](S from, char lo, char hi, S to) {
        for (char c = lo; c <= hi; ++c) {
            dfa_.addTransition(from, c, to);
        }
    };
    
    auto addAllExcept = [&](S from, S to, const std::string& except) {
        for (int i = 1; i < 256; ++i) {
            char c = static_cast<char>(i);
            if (except.find(c) == std::string::npos) {
                dfa_.addTransition(from, c, to);
            }
        }
    };

    // ========== 标识符 ==========
    // Start -> InIdentifier: [a-zA-Z_]
    addRange(S::Start, 'a', 'z', S::InIdentifier);
    addRange(S::Start, 'A', 'Z', S::InIdentifier);
    dfa_.addTransition(S::Start, '_', S::InIdentifier);
    
    // InIdentifier -> InIdentifier: [a-zA-Z0-9_]
    addRange(S::InIdentifier, 'a', 'z', S::InIdentifier);
    addRange(S::InIdentifier, 'A', 'Z', S::InIdentifier);
    addRange(S::InIdentifier, '0', '9', S::InIdentifier);
    dfa_.addTransition(S::InIdentifier, '_', S::InIdentifier);

    // ========== 数字 ==========
    // Start -> InInteger: [1-9]
    addRange(S::Start, '1', '9', S::InInteger);
    
    // Start -> InZero: '0'
    dfa_.addTransition(S::Start, '0', S::InZero);
    
    // InInteger -> InInteger: [0-9]
    addRange(S::InInteger, '0', '9', S::InInteger);
    
    // InZero -> InHexStart: [xX]
    dfa_.addTransition(S::InZero, 'x', S::InHexStart);
    dfa_.addTransition(S::InZero, 'X', S::InHexStart);
    
    // InZero -> InBinaryStart: [bB] (二进制字面量)
    dfa_.addTransition(S::InZero, 'b', S::InBinaryStart);
    dfa_.addTransition(S::InZero, 'B', S::InBinaryStart);
    
    // InZero -> InOctal: [0-7]
    addRange(S::InZero, '0', '7', S::InOctal);
    
    // InZero -> InFloatDot: '.'
    dfa_.addTransition(S::InZero, '.', S::InFloatDot);
    
    // InInteger -> InFloatDot: '.'
    dfa_.addTransition(S::InInteger, '.', S::InFloatDot);
    
    // InHexStart -> InHex: [0-9a-fA-F]
    addRange(S::InHexStart, '0', '9', S::InHex);
    addRange(S::InHexStart, 'a', 'f', S::InHex);
    addRange(S::InHexStart, 'A', 'F', S::InHex);
    
    // InHex -> InHex: [0-9a-fA-F]
    addRange(S::InHex, '0', '9', S::InHex);
    addRange(S::InHex, 'a', 'f', S::InHex);
    addRange(S::InHex, 'A', 'F', S::InHex);
    
    // InBinaryStart -> InBinary: [01]
    dfa_.addTransition(S::InBinaryStart, '0', S::InBinary);
    dfa_.addTransition(S::InBinaryStart, '1', S::InBinary);
    
    // InBinary -> InBinary: [01]
    dfa_.addTransition(S::InBinary, '0', S::InBinary);
    dfa_.addTransition(S::InBinary, '1', S::InBinary);
    
    // InOctal -> InOctal: [0-7]
    addRange(S::InOctal, '0', '7', S::InOctal);
    
    // InFloatDot -> InFloat: [0-9]
    addRange(S::InFloatDot, '0', '9', S::InFloat);
    
    // InFloatDot -> Done: [fFlL] (浮点后缀，支持 5.f 格式)
    dfa_.addTransition(S::InFloatDot, 'f', S::Done);
    dfa_.addTransition(S::InFloatDot, 'F', S::Done);
    dfa_.addTransition(S::InFloatDot, 'l', S::Done);
    dfa_.addTransition(S::InFloatDot, 'L', S::Done);
    
    // InFloat -> InFloat: [0-9]
    addRange(S::InFloat, '0', '9', S::InFloat);
    
    // InFloat -> Done: [fFlL] (浮点后缀)
    dfa_.addTransition(S::InFloat, 'f', S::Done);
    dfa_.addTransition(S::InFloat, 'F', S::Done);
    dfa_.addTransition(S::InFloat, 'l', S::Done);
    dfa_.addTransition(S::InFloat, 'L', S::Done);
    
    // InInteger/InZero/InFloat -> InFloatExp: [eE] (支持 0e0, 123e5 等格式)
    dfa_.addTransition(S::InInteger, 'e', S::InFloatExp);
    dfa_.addTransition(S::InInteger, 'E', S::InFloatExp);
    dfa_.addTransition(S::InZero, 'e', S::InFloatExp);
    dfa_.addTransition(S::InZero, 'E', S::InFloatExp);
    dfa_.addTransition(S::InFloat, 'e', S::InFloatExp);
    dfa_.addTransition(S::InFloat, 'E', S::InFloatExp);
    
    // InFloatExp -> InFloatExpSign: [+-]
    dfa_.addTransition(S::InFloatExp, '+', S::InFloatExpSign);
    dfa_.addTransition(S::InFloatExp, '-', S::InFloatExpSign);
    
    // InFloatExp/InFloatExpSign -> InFloat: [0-9]
    addRange(S::InFloatExp, '0', '9', S::InFloat);
    addRange(S::InFloatExpSign, '0', '9', S::InFloat);
    
    // ========== 整数后缀 ==========
    // 支持: L, LL, U, UL, ULL, LU, LLU (及小写)
    
    // InInteger -> InIntSuffixU: [uU]
    dfa_.addTransition(S::InInteger, 'u', S::InIntSuffixU);
    dfa_.addTransition(S::InInteger, 'U', S::InIntSuffixU);
    // InInteger -> InIntSuffixL: [lL]
    dfa_.addTransition(S::InInteger, 'l', S::InIntSuffixL);
    dfa_.addTransition(S::InInteger, 'L', S::InIntSuffixL);
    
    // InZero -> InIntSuffixU/L
    dfa_.addTransition(S::InZero, 'u', S::InIntSuffixU);
    dfa_.addTransition(S::InZero, 'U', S::InIntSuffixU);
    dfa_.addTransition(S::InZero, 'l', S::InIntSuffixL);
    dfa_.addTransition(S::InZero, 'L', S::InIntSuffixL);
    
    // InOctal -> InIntSuffixU/L
    dfa_.addTransition(S::InOctal, 'u', S::InIntSuffixU);
    dfa_.addTransition(S::InOctal, 'U', S::InIntSuffixU);
    dfa_.addTransition(S::InOctal, 'l', S::InIntSuffixL);
    dfa_.addTransition(S::InOctal, 'L', S::InIntSuffixL);
    
    // InHex -> InIntSuffixU/L
    dfa_.addTransition(S::InHex, 'u', S::InIntSuffixU);
    dfa_.addTransition(S::InHex, 'U', S::InIntSuffixU);
    dfa_.addTransition(S::InHex, 'l', S::InIntSuffixL);
    dfa_.addTransition(S::InHex, 'L', S::InIntSuffixL);
    
    // InBinary -> InIntSuffixU/L
    dfa_.addTransition(S::InBinary, 'u', S::InIntSuffixU);
    dfa_.addTransition(S::InBinary, 'U', S::InIntSuffixU);
    dfa_.addTransition(S::InBinary, 'l', S::InIntSuffixL);
    dfa_.addTransition(S::InBinary, 'L', S::InIntSuffixL);
    
    // InIntSuffixU -> InIntSuffixUL: [lL]
    dfa_.addTransition(S::InIntSuffixU, 'l', S::InIntSuffixUL);
    dfa_.addTransition(S::InIntSuffixU, 'L', S::InIntSuffixUL);
    
    // InIntSuffixL -> InIntSuffixLL: [lL] (for LL)
    dfa_.addTransition(S::InIntSuffixL, 'l', S::InIntSuffixLL);
    dfa_.addTransition(S::InIntSuffixL, 'L', S::InIntSuffixLL);
    // InIntSuffixL -> InIntSuffixUL: [uU] (for LU)
    dfa_.addTransition(S::InIntSuffixL, 'u', S::InIntSuffixUL);
    dfa_.addTransition(S::InIntSuffixL, 'U', S::InIntSuffixUL);
    
    // InIntSuffixUL -> Done: [lL] (for ULL)
    dfa_.addTransition(S::InIntSuffixUL, 'l', S::Done);
    dfa_.addTransition(S::InIntSuffixUL, 'L', S::Done);
    
    // InIntSuffixLL -> Done: [uU] (for LLU)
    dfa_.addTransition(S::InIntSuffixLL, 'u', S::Done);
    dfa_.addTransition(S::InIntSuffixLL, 'U', S::Done);

    // ========== 字符字面量 ==========
    // Start -> InChar: '\''
    dfa_.addTransition(S::Start, '\'', S::InChar);
    
    // InChar -> InCharEnd: 除 '\'' 和 '\\' 外的字符
    addAllExcept(S::InChar, S::InCharEnd, "'\\\n");
    
    // InChar -> InCharEscape: '\\'
    dfa_.addTransition(S::InChar, '\\', S::InCharEscape);
    
    // InCharEscape -> InCharEnd: 普通转义字符 (n, t, r, ', ", \, a, b, f, v, ?)
    for (char c : std::string("ntrabfv'\"\\?")) {
        dfa_.addTransition(S::InCharEscape, c, S::InCharEnd);
    }
    
    // InCharEscape -> InCharOctal1: 八进制数字 [0-7]
    for (char c = '0'; c <= '7'; ++c) {
        dfa_.addTransition(S::InCharEscape, c, S::InCharOctal1);
    }
    
    // InCharEscape -> InCharHexStart: 'x'
    dfa_.addTransition(S::InCharEscape, 'x', S::InCharHexStart);
    
    // InCharOctal1 -> InCharOctal2: 八进制数字 [0-7]
    for (char c = '0'; c <= '7'; ++c) {
        dfa_.addTransition(S::InCharOctal1, c, S::InCharOctal2);
    }
    // InCharOctal1 -> Done: '\''
    dfa_.addTransition(S::InCharOctal1, '\'', S::Done);
    
    // InCharOctal2 -> InCharEnd: 八进制数字 [0-7]
    for (char c = '0'; c <= '7'; ++c) {
        dfa_.addTransition(S::InCharOctal2, c, S::InCharEnd);
    }
    // InCharOctal2 -> Done: '\''
    dfa_.addTransition(S::InCharOctal2, '\'', S::Done);
    
    // InCharHexStart -> InCharHex1: 十六进制数字 [0-9a-fA-F]
    for (char c = '0'; c <= '9'; ++c) {
        dfa_.addTransition(S::InCharHexStart, c, S::InCharHex1);
    }
    for (char c = 'a'; c <= 'f'; ++c) {
        dfa_.addTransition(S::InCharHexStart, c, S::InCharHex1);
    }
    for (char c = 'A'; c <= 'F'; ++c) {
        dfa_.addTransition(S::InCharHexStart, c, S::InCharHex1);
    }
    
    // InCharHex1 -> InCharEnd: 十六进制数字 [0-9a-fA-F]
    for (char c = '0'; c <= '9'; ++c) {
        dfa_.addTransition(S::InCharHex1, c, S::InCharEnd);
    }
    for (char c = 'a'; c <= 'f'; ++c) {
        dfa_.addTransition(S::InCharHex1, c, S::InCharEnd);
    }
    for (char c = 'A'; c <= 'F'; ++c) {
        dfa_.addTransition(S::InCharHex1, c, S::InCharEnd);
    }
    // InCharHex1 -> Done: '\''
    dfa_.addTransition(S::InCharHex1, '\'', S::Done);
    
    // InCharEnd -> Done: '\''
    dfa_.addTransition(S::InCharEnd, '\'', S::Done);

    // ========== 字符串字面量 ==========
    // Start -> InString: '"'
    dfa_.addTransition(S::Start, '"', S::InString);
    
    // InString -> InString: 除 '"' 和 '\\' 外的字符
    addAllExcept(S::InString, S::InString, "\"\\\n");
    
    // InString -> InStringEscape: '\\'
    dfa_.addTransition(S::InString, '\\', S::InStringEscape);
    
    // InStringEscape -> InString: 普通转义字符
    for (char c : std::string("ntrabfv'\"\\?")) {
        dfa_.addTransition(S::InStringEscape, c, S::InString);
    }
    
    // InStringEscape -> InStringOctal1: 八进制数字 [0-7]
    for (char c = '0'; c <= '7'; ++c) {
        dfa_.addTransition(S::InStringEscape, c, S::InStringOctal1);
    }
    
    // InStringEscape -> InStringHexStart: 'x'
    dfa_.addTransition(S::InStringEscape, 'x', S::InStringHexStart);
    
    // InStringOctal1 -> InStringOctal2: 八进制数字 [0-7]
    for (char c = '0'; c <= '7'; ++c) {
        dfa_.addTransition(S::InStringOctal1, c, S::InStringOctal2);
    }
    // InStringOctal1 回到 InString 或结束
    addAllExcept(S::InStringOctal1, S::InString, "01234567\"\\\n");
    dfa_.addTransition(S::InStringOctal1, '"', S::Done);
    dfa_.addTransition(S::InStringOctal1, '\\', S::InStringEscape);
    
    // InStringOctal2 -> InString: 八进制数字 [0-7] 或其他字符
    for (char c = '0'; c <= '7'; ++c) {
        dfa_.addTransition(S::InStringOctal2, c, S::InString);
    }
    addAllExcept(S::InStringOctal2, S::InString, "01234567\"\\\n");
    dfa_.addTransition(S::InStringOctal2, '"', S::Done);
    dfa_.addTransition(S::InStringOctal2, '\\', S::InStringEscape);
    
    // InStringHexStart -> InStringHex1: 十六进制数字
    for (char c = '0'; c <= '9'; ++c) {
        dfa_.addTransition(S::InStringHexStart, c, S::InStringHex1);
    }
    for (char c = 'a'; c <= 'f'; ++c) {
        dfa_.addTransition(S::InStringHexStart, c, S::InStringHex1);
    }
    for (char c = 'A'; c <= 'F'; ++c) {
        dfa_.addTransition(S::InStringHexStart, c, S::InStringHex1);
    }
    
    // InStringHex1 -> InString: 十六进制数字或其他字符
    for (char c = '0'; c <= '9'; ++c) {
        dfa_.addTransition(S::InStringHex1, c, S::InString);
    }
    for (char c = 'a'; c <= 'f'; ++c) {
        dfa_.addTransition(S::InStringHex1, c, S::InString);
    }
    for (char c = 'A'; c <= 'F'; ++c) {
        dfa_.addTransition(S::InStringHex1, c, S::InString);
    }
    addAllExcept(S::InStringHex1, S::InString, "0123456789abcdefABCDEF\"\\\n");
    dfa_.addTransition(S::InStringHex1, '"', S::Done);
    dfa_.addTransition(S::InStringHex1, '\\', S::InStringEscape);
    
    // InString -> Done: '"'
    dfa_.addTransition(S::InString, '"', S::Done);

    // ========== 注释 ==========
    // Start -> InSlash: '/'
    dfa_.addTransition(S::Start, '/', S::InSlash);
    
    // InSlash -> InLineComment: '/'
    dfa_.addTransition(S::InSlash, '/', S::InLineComment);
    
    // InSlash -> InBlockComment: '*'
    dfa_.addTransition(S::InSlash, '*', S::InBlockComment);
    
    // InSlash -> Done: '='
    dfa_.addTransition(S::InSlash, '=', S::Done);
    
    // InLineComment -> InLineComment: 除 '\n' 外
    addAllExcept(S::InLineComment, S::InLineComment, "\n");
    
    // InBlockComment -> InBlockComment: 除 '*' 外
    addAllExcept(S::InBlockComment, S::InBlockComment, "*");
    
    // InBlockComment -> InBlockCommentStar: '*'
    dfa_.addTransition(S::InBlockComment, '*', S::InBlockCommentStar);
    
    // InBlockCommentStar -> InBlockComment: 除 '/' 和 '*' 外
    addAllExcept(S::InBlockCommentStar, S::InBlockComment, "/*");
    
    // InBlockCommentStar -> InBlockCommentStar: '*'
    dfa_.addTransition(S::InBlockCommentStar, '*', S::InBlockCommentStar);
    
    // InBlockCommentStar -> Done: '/'
    dfa_.addTransition(S::InBlockCommentStar, '/', S::Done);

    // ========== 运算符 ==========
    // + ++ +=
    dfa_.addTransition(S::Start, '+', S::InPlus);
    dfa_.addTransition(S::InPlus, '+', S::Done);
    dfa_.addTransition(S::InPlus, '=', S::Done);
    
    // - -- -= ->
    dfa_.addTransition(S::Start, '-', S::InMinus);
    dfa_.addTransition(S::InMinus, '-', S::Done);
    dfa_.addTransition(S::InMinus, '=', S::Done);
    dfa_.addTransition(S::InMinus, '>', S::Done);
    
    // * *=
    dfa_.addTransition(S::Start, '*', S::InStar);
    dfa_.addTransition(S::InStar, '=', S::Done);
    
    // % %=
    dfa_.addTransition(S::Start, '%', S::InPercent);
    dfa_.addTransition(S::InPercent, '=', S::Done);
    
    // = ==
    dfa_.addTransition(S::Start, '=', S::InEqual);
    dfa_.addTransition(S::InEqual, '=', S::Done);
    
    // ! !=
    dfa_.addTransition(S::Start, '!', S::InExclaim);
    dfa_.addTransition(S::InExclaim, '=', S::Done);
    
    // < <= << <<=
    dfa_.addTransition(S::Start, '<', S::InLess);
    dfa_.addTransition(S::InLess, '=', S::Done);
    dfa_.addTransition(S::InLess, '<', S::InLessLess);
    dfa_.addTransition(S::InLessLess, '=', S::Done);
    
    // > >= >> >>=
    dfa_.addTransition(S::Start, '>', S::InGreater);
    dfa_.addTransition(S::InGreater, '=', S::Done);
    dfa_.addTransition(S::InGreater, '>', S::InGreaterGreater);
    dfa_.addTransition(S::InGreaterGreater, '=', S::Done);
    
    // & && &=
    dfa_.addTransition(S::Start, '&', S::InAmp);
    dfa_.addTransition(S::InAmp, '&', S::Done);
    dfa_.addTransition(S::InAmp, '=', S::Done);
    
    // | || |=
    dfa_.addTransition(S::Start, '|', S::InPipe);
    dfa_.addTransition(S::InPipe, '|', S::Done);
    dfa_.addTransition(S::InPipe, '=', S::Done);
    
    // ^ ^=
    dfa_.addTransition(S::Start, '^', S::InCaret);
    dfa_.addTransition(S::InCaret, '=', S::Done);
    
    // . ...
    dfa_.addTransition(S::Start, '.', S::InDot);
    dfa_.addTransition(S::InDot, '.', S::InDotDot);
    dfa_.addTransition(S::InDotDot, '.', S::Done);
    // . 后跟数字变成浮点
    addRange(S::InDot, '0', '9', S::InFloat);

    // ========== 单字符运算符/分隔符（直接进入 Done）==========
    // 这些在 scanToken 中特殊处理
}

//=============================================================================
// 获取回退状态
//=============================================================================

LexerState Lexer::getFallbackState(LexerState s) const {
    switch (s) {
        case LexerState::Start:
            return LexerState::Error;
        
        // 标识符完成
        case LexerState::InIdentifier:
            return LexerState::Done;
        
        // 数字完成
        case LexerState::InInteger:
        case LexerState::InZero:
        case LexerState::InOctal:
        case LexerState::InHex:
        case LexerState::InFloat:
        case LexerState::InBinary:
        case LexerState::InIntSuffixU:
        case LexerState::InIntSuffixL:
        case LexerState::InIntSuffixUL:
        case LexerState::InIntSuffixLL:
            return LexerState::Done;
        
        // 浮点数部分状态 - 错误或需要特殊处理
        case LexerState::InHexStart:
        case LexerState::InBinaryStart:
        case LexerState::InFloatExp:
        case LexerState::InFloatExpSign:
            return LexerState::Error;
        
        // 小数点可能是单独的 '.'
        case LexerState::InFloatDot:
            return LexerState::Done;
        
        // 字符/字符串未完成
        case LexerState::InChar:
        case LexerState::InCharEscape:
        case LexerState::InCharEnd:
        case LexerState::InCharOctal1:
        case LexerState::InCharOctal2:
        case LexerState::InCharHexStart:
        case LexerState::InCharHex1:
        case LexerState::InString:
        case LexerState::InStringEscape:
        case LexerState::InStringOctal1:
        case LexerState::InStringOctal2:
        case LexerState::InStringHexStart:
        case LexerState::InStringHex1:
            return LexerState::Error;
        
        // 注释
        case LexerState::InLineComment:
            return LexerState::Done;  // 行注释遇到 EOF 结束
        case LexerState::InBlockComment:
        case LexerState::InBlockCommentStar:
            return LexerState::Error;  // 块注释未闭合
        
        // 运算符可以在任何时候结束
        case LexerState::InSlash:
        case LexerState::InPlus:
        case LexerState::InMinus:
        case LexerState::InStar:
        case LexerState::InPercent:
        case LexerState::InEqual:
        case LexerState::InExclaim:
        case LexerState::InLess:
        case LexerState::InGreater:
        case LexerState::InLessLess:
        case LexerState::InGreaterGreater:
        case LexerState::InAmp:
        case LexerState::InPipe:
        case LexerState::InCaret:
        case LexerState::InDot:
            return LexerState::Done;
        
        case LexerState::InDotDot:
            return LexerState::Error;  // '..' 不是有效 token
        
        case LexerState::Done:
            return LexerState::Done;
        
        default:
            return LexerState::Error;
    }
}

//=============================================================================
// 字符操作
//=============================================================================

SourceLocation Lexer::currentLocation() const {
    return SourceLocation(filename_, line_, column_, static_cast<uint32_t>(pos_));
}

char Lexer::peek(size_t offset) const {
    size_t idx = pos_ + offset;
    return idx < source_.size() ? source_[idx] : '\0';
}

char Lexer::advance() {
    if (pos_ >= source_.size()) return '\0';

    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

void Lexer::skipWhitespace() {
    while (!isEof()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else {
            break;
        }
    }
}

//=============================================================================
// Token 扫描
//=============================================================================

Token Lexer::nextToken() {
    if (hasCachedToken_) {
        hasCachedToken_ = false;
        return cachedToken_;
    }
    return scanToken();
}

Token Lexer::peekToken() {
    if (!hasCachedToken_) {
        cachedToken_ = scanToken();
        hasCachedToken_ = true;
    }
    return cachedToken_;
}

/**
 * @brief DFA 主循环，扫描下一个 Token
 * 
 * 处理流程：
 * 1. 跳过空白字符和注释
 * 2. 检查 EOF
 * 3. 处理单字符分隔符（不需要状态机）
 * 4. 使用 DFA 状态机扫描多字符 Token
 * 5. 根据终止状态构造 Token
 */
Token Lexer::scanToken() {
    // 1. 跳过空白和注释
    while (true) {
        skipWhitespace();
        
        // 2. 检查 EOF
        if (isEof()) {
            Token tok(TokenKind::EndOfFile, currentLocation());
            return tok;
        }
        
        // 跳过注释
        if (peek() == '/' && (peek(1) == '/' || peek(1) == '*')) {
            if (peek(1) == '/') {
                // 单行注释：跳到行尾
                advance(); advance();
                while (!isEof() && peek() != '\n') {
                    advance();
                }
            } else {
                // 块注释：跳到 */
                advance(); advance();
                while (!isEof()) {
                    if (peek() == '*' && peek(1) == '/') {
                        advance(); advance();
                        break;
                    }
                    advance();
                }
            }
            continue;
        }
        
        break;
    }

    // 记录 Token 起始位置
    tokenStart_ = pos_;
    tokenLoc_ = currentLocation();
    lexeme_.clear();
    
    // 处理单字符分隔符（不需要状态机）
    char c = peek();
    TokenKind singleCharKind = TokenKind::Invalid;
    switch (c) {
        case '~': singleCharKind = TokenKind::Tilde; break;
        case '?': singleCharKind = TokenKind::Question; break;
        case ':': singleCharKind = TokenKind::Colon; break;
        case ';': singleCharKind = TokenKind::Semicolon; break;
        case ',': singleCharKind = TokenKind::Comma; break;
        case '(': singleCharKind = TokenKind::LeftParen; break;
        case ')': singleCharKind = TokenKind::RightParen; break;
        case '[': singleCharKind = TokenKind::LeftBracket; break;
        case ']': singleCharKind = TokenKind::RightBracket; break;
        case '{': singleCharKind = TokenKind::LeftBrace; break;
        case '}': singleCharKind = TokenKind::RightBrace; break;
        default: break;
    }
    
    if (singleCharKind != TokenKind::Invalid) {
        advance();
        Token tok(singleCharKind, tokenLoc_);
        tok.text = source_.substr(tokenStart_, 1);
        return tok;
    }

    // 使用状态机扫描
    dfa_.reset();
    
    while (true) {
        c = peek();
        
        if (c == '\0' || !dfa_.step(c)) {
            // 无法转移，检查是否可以完成
            LexerState fallback = getFallbackState(dfa_.current());
            
            if (fallback == LexerState::Done) {
                return finalizeToken(dfa_.current());
            } else if (fallback == LexerState::Error) {
                if (lexeme_.empty()) {
                    // 非法字符
                    advance();
                    Token tok(TokenKind::Invalid, tokenLoc_);
                    tok.stringValue = std::string("Unexpected character: ") + c;
                    return tok;
                }
                // 部分 token 后出错
                Token tok(TokenKind::Invalid, tokenLoc_);
                tok.stringValue = "Incomplete token: " + lexeme_;
                return tok;
            }
            break;
        }
        
        // 转移成功，消费字符
        lexeme_ += advance();
        
        // 如果到达 Done 状态，立即返回
        if (dfa_.current() == LexerState::Done) {
            return finalizeToken(dfa_.current());
        }
    }
    
    // 不应该到达这里
    Token tok(TokenKind::Invalid, tokenLoc_);
    tok.stringValue = "Unexpected end of input";
    return tok;
}

//=============================================================================
// 完成 Token
//=============================================================================

Token Lexer::finalizeToken(LexerState state) {
    Token tok(TokenKind::Invalid, tokenLoc_);
    tok.text = source_.substr(tokenStart_, pos_ - tokenStart_);
    
    switch (state) {
        case LexerState::InIdentifier:
        case LexerState::Done: {
            // 检查状态机历史来判断 token 类型
            if (state == LexerState::InIdentifier || 
                (lexeme_.length() > 0 && isAlpha(lexeme_[0]))) {
                // 检查是否是关键字
                TokenKind kwKind = keywordToTokenKind(lexeme_);
                if (kwKind != TokenKind::Invalid) {
                    tok.kind = kwKind;
                } else {
                    tok.kind = TokenKind::Identifier;
                    tok.stringValue = lexeme_;
                }
            } else if (lexeme_ == "...") {
                tok.kind = TokenKind::Ellipsis;
            } else if (lexeme_.length() >= 2 && lexeme_[0] == '\'') {
                // 字符字面量 'x' 或 '\x' 或 '\101' 或 '\x41'
                tok.kind = TokenKind::CharLiteral;
                tok.charValue = parseCharLiteral(lexeme_);
            } else if (lexeme_.length() >= 2 && lexeme_[0] == '"') {
                // 字符串字面量
                tok.kind = TokenKind::StringLiteral;
                tok.stringValue = processStringEscapes(lexeme_.substr(1, lexeme_.length() - 2));
            } else if (lexeme_.length() > 0 && (isDigit(lexeme_[0]) || (lexeme_[0] == '.' && lexeme_.length() > 1 && isDigit(lexeme_[1])))) {
                // 带后缀的浮点数 (例如 0.5f 或 .5f)
                tok.kind = TokenKind::FloatLiteral;
                tok.floatValue = parseFloat(lexeme_);
            } else if (lexeme_.length() > 0) {
                // 根据 lexeme 判断运算符类型
                tok = finalizeOperator();
            }
            break;
        }
        
        case LexerState::InInteger:
        case LexerState::InZero:
        case LexerState::InIntSuffixU:
        case LexerState::InIntSuffixL:
        case LexerState::InIntSuffixUL:
        case LexerState::InIntSuffixLL: {
            tok.kind = TokenKind::IntLiteral;
            tok.intValue = parseInteger(lexeme_, 10);
            break;
        }
        
        case LexerState::InOctal: {
            tok.kind = TokenKind::IntLiteral;
            tok.intValue = parseInteger(lexeme_, 8);
            break;
        }
        
        case LexerState::InHex: {
            tok.kind = TokenKind::IntLiteral;
            tok.intValue = parseInteger(lexeme_, 16);
            break;
        }
        
        case LexerState::InBinary: {
            tok.kind = TokenKind::IntLiteral;
            tok.intValue = parseInteger(lexeme_, 2);
            break;
        }
        
        case LexerState::InFloat:
        case LexerState::InFloatDot: {
            if (lexeme_ == ".") {
                tok.kind = TokenKind::Dot;
            } else {
                tok.kind = TokenKind::FloatLiteral;
                tok.floatValue = parseFloat(lexeme_);
            }
            break;
        }
        
        // 运算符状态
        case LexerState::InSlash:
        case LexerState::InPlus:
        case LexerState::InMinus:
        case LexerState::InStar:
        case LexerState::InPercent:
        case LexerState::InEqual:
        case LexerState::InExclaim:
        case LexerState::InLess:
        case LexerState::InGreater:
        case LexerState::InLessLess:
        case LexerState::InGreaterGreater:
        case LexerState::InAmp:
        case LexerState::InPipe:
        case LexerState::InCaret:
        case LexerState::InDot:
            tok = finalizeOperator();
            break;
        
        case LexerState::InCharEnd:
        case LexerState::InCharOctal1:
        case LexerState::InCharOctal2:
        case LexerState::InCharHex1:
            // 字符字面量
            tok.kind = TokenKind::CharLiteral;
            tok.charValue = parseCharLiteral(lexeme_);
            // 消费结束的 '
            advance();
            break;
        
        case LexerState::InLineComment:
            // 注释，递归获取下一个 token
            return scanToken();
        
        default:
            tok.kind = TokenKind::Invalid;
            tok.stringValue = "Unexpected state in finalizeToken";
            break;
    }
    
    return tok;
}

Token Lexer::finalizeOperator() {
    Token tok(TokenKind::Invalid, tokenLoc_);
    tok.text = source_.substr(tokenStart_, pos_ - tokenStart_);
    
    // 运算符映射表
    static const std::unordered_map<std::string, TokenKind> operatorMap = {
        // 算术
        {"+", TokenKind::Plus},
        {"-", TokenKind::Minus},
        {"*", TokenKind::Star},
        {"/", TokenKind::Slash},
        {"%", TokenKind::Percent},
        
        // 自增自减
        {"++", TokenKind::PlusPlus},
        {"--", TokenKind::MinusMinus},
        
        // 比较
        {"==", TokenKind::EqualEqual},
        {"!=", TokenKind::ExclaimEqual},
        {"<", TokenKind::Less},
        {">", TokenKind::Greater},
        {"<=", TokenKind::LessEqual},
        {">=", TokenKind::GreaterEqual},
        
        // 逻辑
        {"&&", TokenKind::AmpAmp},
        {"||", TokenKind::PipePipe},
        {"!", TokenKind::Exclaim},
        
        // 位运算
        {"&", TokenKind::Amp},
        {"|", TokenKind::Pipe},
        {"^", TokenKind::Caret},
        {"~", TokenKind::Tilde},
        {"<<", TokenKind::LessLess},
        {">>", TokenKind::GreaterGreater},
        
        // 赋值
        {"=", TokenKind::Equal},
        {"+=", TokenKind::PlusEqual},
        {"-=", TokenKind::MinusEqual},
        {"*=", TokenKind::StarEqual},
        {"/=", TokenKind::SlashEqual},
        {"%=", TokenKind::PercentEqual},
        {"&=", TokenKind::AmpEqual},
        {"|=", TokenKind::PipeEqual},
        {"^=", TokenKind::CaretEqual},
        {"<<=", TokenKind::LessLessEqual},
        {">>=", TokenKind::GreaterGreaterEqual},
        
        // 成员访问
        {".", TokenKind::Dot},
        {"->", TokenKind::Arrow},
        
        // 其他
        {"...", TokenKind::Ellipsis},
    };
    
    auto it = operatorMap.find(lexeme_);
    if (it != operatorMap.end()) {
        tok.kind = it->second;
    } else {
        tok.kind = TokenKind::Invalid;
        tok.stringValue = "Unknown operator: " + lexeme_;
    }
    
    return tok;
}

//=============================================================================
// 辅助函数
//=============================================================================

char Lexer::processEscape(char c) const {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return '\0';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        default: return c;
    }
}

char Lexer::parseCharLiteral(const std::string& s) const {
    // s 格式: 'x' 或 '\n' 或 '\101' 或 '\x41'
    if (s.length() < 2) return '\0';
    
    if (s[1] != '\\') {
        // 普通字符 'x'
        return s[1];
    }
    
    // 转义字符
    if (s.length() < 3) return '\0';
    
    char escChar = s[2];
    
    // 八进制转义 \0, \01, \012, \0123
    if (escChar >= '0' && escChar <= '7') {
        int value = 0;
        size_t i = 2;
        while (i < s.length() && s[i] >= '0' && s[i] <= '7' && i < 5) {
            value = value * 8 + (s[i] - '0');
            ++i;
        }
        return static_cast<char>(value);
    }
    
    // 十六进制转义 \x41
    if (escChar == 'x' && s.length() >= 4) {
        int value = 0;
        size_t i = 3;
        while (i < s.length() && i < 5) {
            char c = s[i];
            if (c >= '0' && c <= '9') {
                value = value * 16 + (c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value = value * 16 + (c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value = value * 16 + (c - 'A' + 10);
            } else {
                break;
            }
            ++i;
        }
        return static_cast<char>(value);
    }
    
    // 普通转义字符
    return processEscape(escChar);
}

std::string Lexer::processStringEscapes(const std::string& s) const {
    std::string result;
    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '\\' && i + 1 < s.length()) {
            char escChar = s[i + 1];
            // 八进制转义
            if (escChar >= '0' && escChar <= '7') {
                int value = 0;
                size_t j = i + 1;
                int count = 0;
                while (j < s.length() && s[j] >= '0' && s[j] <= '7' && count < 3) {
                    value = value * 8 + (s[j] - '0');
                    ++j;
                    ++count;
                }
                result += static_cast<char>(value);
                i = j - 1;
            }
            // 十六进制转义
            else if (escChar == 'x' && i + 2 < s.length()) {
                int value = 0;
                size_t j = i + 2;
                int count = 0;
                while (j < s.length() && count < 2) {
                    char c = s[j];
                    if (c >= '0' && c <= '9') {
                        value = value * 16 + (c - '0');
                    } else if (c >= 'a' && c <= 'f') {
                        value = value * 16 + (c - 'a' + 10);
                    } else if (c >= 'A' && c <= 'F') {
                        value = value * 16 + (c - 'A' + 10);
                    } else {
                        break;
                    }
                    ++j;
                    ++count;
                }
                result += static_cast<char>(value);
                i = j - 1;
            }
            // 普通转义
            else {
                result += processEscape(s[++i]);
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

int64_t Lexer::parseInteger(const std::string& s, int base) const {
    std::string numStr = s;
    
    // 移除后缀
    while (!numStr.empty() && (numStr.back() == 'u' || numStr.back() == 'U' ||
                                numStr.back() == 'l' || numStr.back() == 'L')) {
        numStr.pop_back();
    }
    
    // 移除前缀（用于十六进制和二进制）
    if (base == 16 && numStr.length() > 2 && numStr[0] == '0' && 
        (numStr[1] == 'x' || numStr[1] == 'X')) {
        numStr = numStr.substr(2);
    } else if (base == 2 && numStr.length() > 2 && numStr[0] == '0' && 
        (numStr[1] == 'b' || numStr[1] == 'B')) {
        numStr = numStr.substr(2);
    }
    
    return std::strtoll(numStr.c_str(), nullptr, base);
}

double Lexer::parseFloat(const std::string& s) const {
    std::string numStr = s;
    
    // 移除后缀
    if (!numStr.empty() && (numStr.back() == 'f' || numStr.back() == 'F' ||
                            numStr.back() == 'l' || numStr.back() == 'L')) {
        numStr.pop_back();
    }
    
    return std::strtod(numStr.c_str(), nullptr);
}

} // namespace cdd
