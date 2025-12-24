/**
 * @file Preprocessor.cpp
 * @brief 预处理器实现
 * @author mozihe
 */

#include "Preprocessor.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <cstdlib>
#include <regex>

namespace fs = std::filesystem;

Preprocessor::Preprocessor() {
    initIncludePaths();
}

void Preprocessor::initIncludePaths() {
    // 1. 从环境变量 CDD_INCLUDE_PATH 读取路径
    const char* envPath = std::getenv("CDD_INCLUDE_PATH");
    if (envPath) {
        std::string pathStr(envPath);
        size_t start = 0;
        size_t end;
        while ((end = pathStr.find(':', start)) != std::string::npos) {
            std::string path = pathStr.substr(start, end - start);
            if (!path.empty()) {
                includePaths_.push_back(path);
            }
            start = end + 1;
        }
        // 最后一段
        if (start < pathStr.length()) {
            std::string path = pathStr.substr(start);
            if (!path.empty()) {
                includePaths_.push_back(path);
            }
        }
    }
    
    // 2. 添加 CDD 标准库路径
    // 从 CDD_STDLIB_PATH 环境变量
    const char* stdlibPath = std::getenv("CDD_STDLIB_PATH");
    if (stdlibPath && fs::exists(stdlibPath)) {
        includePaths_.push_back(stdlibPath);
    }
    
    // 常见安装位置
    std::vector<std::string> stdlibCandidates = {
        "/usr/local/include/cdd",       // 标准安装位置
        "/usr/include/cdd",             // 系统安装位置
        "/opt/cdd/include",             // 可选安装位置
        "../stdlib",                     // 开发时相对路径
        "stdlib"                         // 当前目录下
    };
    
    for (const auto& path : stdlibCandidates) {
        if (fs::exists(path)) {
            includePaths_.push_back(path);
            break;  // 只添加第一个存在的
        }
    }
    
    // 3. 添加系统默认路径
    includePaths_.push_back("/usr/local/include");
    includePaths_.push_back("/usr/include");
}

void Preprocessor::addIncludePath(const std::string& path) {
    // 插入到系统路径之前
    if (includePaths_.size() >= 2) {
        includePaths_.insert(includePaths_.end() - 2, path);
    } else {
        includePaths_.insert(includePaths_.begin(), path);
    }
}

void Preprocessor::defineMacro(const std::string& name, const std::string& value) {
    MacroDef def;
    def.isFunctionLike = false;
    def.body = value;
    macros_[name] = def;
}

std::string Preprocessor::resolveIncludePath(const std::string& headerName, bool isSystemHeader) {
    // 1. 如果是绝对路径，直接返回
    if (!headerName.empty() && headerName[0] == '/') {
        if (fs::exists(headerName)) {
            return headerName;
        }
        return "";
    }
    
    // 2. 对于 "" 包含，先在当前目录搜索
    if (!isSystemHeader && !currentDir_.empty()) {
        std::string path = currentDir_ + "/" + headerName;
        if (fs::exists(path)) {
            return fs::absolute(path).string();
        }
    }
    
    // 3. 在 include 路径中搜索
    for (const auto& dir : includePaths_) {
        std::string path = dir + "/" + headerName;
        if (fs::exists(path)) {
            return fs::absolute(path).string();
        }
    }
    
    // 4. 最后尝试当前工作目录
    if (fs::exists(headerName)) {
        return fs::absolute(headerName).string();
    }
    
    return "";
}

std::string Preprocessor::preprocess(const std::string& filename) {
    loadedFiles_.clear();
    macros_.clear();
    // 清空条件编译栈
    while (!conditionStack_.empty()) conditionStack_.pop();
    
    std::stringstream ss;
    
    // 保存源文件所在目录作为首要搜索路径
    try {
        fs::path filePath = fs::absolute(filename);
        currentDir_ = filePath.parent_path().string();
    } catch (...) {
        currentDir_ = ".";
    }
    
    processFile(filename, ss);
    
    // 检查条件编译是否完整闭合
    if (!conditionStack_.empty()) {
        throw std::runtime_error("Preprocessor Error: Unterminated conditional directive (#if/#ifdef without #endif)");
    }
    
    return ss.str();
}

/**
 * @brief 处理单个源文件
 * 
 * 处理流程：
 * 1. 检查循环包含（已加载则跳过）
 * 2. 保存/恢复目录上下文
 * 3. 逐行处理：
 *    a. 处理续行符
 *    b. 处理条件编译指令
 *    c. 处理 #include
 *    d. 处理 #define / #undef
 *    e. 展开宏并输出
 */
void Preprocessor::processFile(const std::string& filename, std::stringstream& out) {
    // 1. 检查循环包含
    std::string absPath;
    try {
        absPath = fs::absolute(filename).string();
    } catch (...) {
        absPath = filename;
    }

    if (loadedFiles_.count(absPath)) return;
    loadedFiles_.insert(absPath);
    
    // 2. 保存当前目录上下文
    std::string savedDir = currentDir_;
    try {
        fs::path filePath(absPath);
        currentDir_ = filePath.parent_path().string();
    } catch (...) {}

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Preprocessor Error: Cannot open file '" + filename + "'");
    }

    // 3. 逐行处理
    std::string line;
    while (std::getline(file, line)) {
        // 3a. 处理续行符 '\'
        while (!line.empty()) {
            if (line.back() == '\r') line.pop_back();
            else if (line.back() == '\\') {
                line.pop_back();
                std::string nextLine;
                if (std::getline(file, nextLine)) {
                    line += nextLine;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        // 3b. 处理条件编译指令（无论当前分支是否激活）
        if (handleConditional(line, out)) {
            continue;
        }

        // 如果当前分支不激活，跳过后续处理
        if (!isCurrentBranchActive()) {
            continue;
        }

        // 3c. #include
        auto [isInclude, headerInfo] = parseInclude(line);
        if (isInclude) {
            std::string headerName = headerInfo.first;
            bool isSystemHeader = headerInfo.second;
            
            std::string resolvedPath = resolveIncludePath(headerName, isSystemHeader);
            if (resolvedPath.empty()) {
                throw std::runtime_error("Preprocessor Error: Cannot find header '" + headerName + "'");
            }
            
            out << "// --- Begin include: " << headerName << " ---\n";
            processFile(resolvedPath, out);
            out << "\n// --- End include: " << headerName << " ---\n";
            continue;
        }

        // 4. #define
        if (parseDefine(line)) {
            out << "// [Defined]: " << line << "\n";
            continue;
        }

        // 5. #undef
        if (parseUndef(line)) {
            out << "// [Undefined]: " << line << "\n";
            continue;
        }

        // 6. 宏展开
        std::string trimmed = trim(line);
        if (!trimmed.empty() && trimmed[0] == '#') {
            out << "// [Ignored Preprocessor]: " << line << "\n";
        } else {
            // 初始调用，forbidden 为空
            std::string expanded = expandMacros(line);
            out << expanded << "\n";
        }
    }
    // 恢复目录上下文
    currentDir_ = savedDir;
}

std::pair<bool, std::pair<std::string, bool>> Preprocessor::parseInclude(const std::string& line) {
    std::string s = trim(line);
    if (s.find("#include") != 0) return {false, {"", false}};
    std::string content = trim(s.substr(8));
    if (content.empty()) return {false, {"", false}};
    char startQuote = content[0];
    bool isSystemHeader = (startQuote == '<');
    char endQuote = (startQuote == '"') ? '"' : (startQuote == '<' ? '>' : 0);
    if (!endQuote) return {false, {"", false}};
    size_t endPos = content.find(endQuote, 1);
    if (endPos == std::string::npos) return {false, {"", false}};
    return {true, {content.substr(1, endPos - 1), isSystemHeader}};
}

bool Preprocessor::parseDefine(const std::string& line) {
    std::string s = trim(line);
    if (s.find("#define") != 0) return false;

    size_t keyStart = s.find_first_not_of(" \t", 7);
    if (keyStart == std::string::npos) return false;

    size_t keyEnd = keyStart;
    while (keyEnd < s.length()) {
        char c = s[keyEnd];
        if (c == '(' || std::isspace(static_cast<unsigned char>(c))) break;
        keyEnd++;
    }

    std::string key = s.substr(keyStart, keyEnd - keyStart);
    if (key.empty()) return false;

    MacroDef def;

    // 函数式宏解析
    if (keyEnd < s.length() && s[keyEnd] == '(') {
        def.isFunctionLike = true;
        size_t paramStart = keyEnd + 1;
        size_t paramEnd = s.find(')', paramStart);
        if (paramEnd == std::string::npos) return false;

        std::string paramsStr = s.substr(paramStart, paramEnd - paramStart);
        std::stringstream pss(paramsStr);
        std::string param;
        while (std::getline(pss, param, ',')) {
            def.params.push_back(trim(param));
        }
        keyEnd = paramEnd + 1;
    }

    std::string body = (keyEnd < s.length()) ? trim(s.substr(keyEnd)) : "";
    
    // 移除行尾的 // 注释
    size_t commentPos = std::string::npos;
    bool inString = false;
    char stringChar = 0;
    for (size_t i = 0; i < body.length(); ++i) {
        char c = body[i];
        if (!inString) {
            if (c == '"' || c == '\'') {
                inString = true;
                stringChar = c;
            } else if (c == '/' && i + 1 < body.length() && body[i + 1] == '/') {
                commentPos = i;
                break;
            }
        } else {
            if (c == '\\' && i + 1 < body.length()) {
                ++i;  // 跳过转义字符
            } else if (c == stringChar) {
                inString = false;
            }
        }
    }
    if (commentPos != std::string::npos) {
        body = trim(body.substr(0, commentPos));
    }
    
    def.body = body;

    macros_[key] = def;
    return true;
}

bool Preprocessor::parseUndef(const std::string& line) {
    std::string s = trim(line);
    if (s.find("#undef") != 0) return false;
    
    size_t nameStart = s.find_first_not_of(" \t", 6);
    if (nameStart == std::string::npos) return false;
    
    size_t nameEnd = s.find_first_of(" \t\r\n", nameStart);
    std::string name = (nameEnd == std::string::npos) 
        ? s.substr(nameStart) 
        : s.substr(nameStart, nameEnd - nameStart);
    
    if (!name.empty()) {
        macros_.erase(name);
        return true;
    }
    return false;
}

bool Preprocessor::isCurrentBranchActive() const {
    if (conditionStack_.empty()) {
        return true;  // 没有条件编译时，默认激活
    }
    return conditionStack_.top().active;
}

bool Preprocessor::handleConditional(const std::string& line, std::stringstream& out) {
    std::string s = trim(line);
    if (s.empty() || s[0] != '#') return false;
    
    // 解析指令
    size_t directiveEnd = s.find_first_of(" \t", 1);
    std::string directive = (directiveEnd == std::string::npos) 
        ? s.substr(1) 
        : s.substr(1, directiveEnd - 1);
    
    std::string rest = (directiveEnd == std::string::npos) 
        ? "" 
        : trim(s.substr(directiveEnd));
    
    // #ifdef
    if (directive == "ifdef") {
        bool parentActive = isCurrentBranchActive();
        bool macroExists = macros_.count(rest) > 0;
        bool shouldActivate = parentActive && macroExists;
        
        ConditionalState state;
        state.active = shouldActivate;
        state.hasMatched = shouldActivate;
        state.parentActive = parentActive;
        conditionStack_.push(state);
        
        out << "// [Conditional]: #ifdef " << rest 
            << (shouldActivate ? " (active)" : " (inactive)") << "\n";
        return true;
    }
    
    // #ifndef
    if (directive == "ifndef") {
        bool parentActive = isCurrentBranchActive();
        bool macroExists = macros_.count(rest) > 0;
        bool shouldActivate = parentActive && !macroExists;
        
        ConditionalState state;
        state.active = shouldActivate;
        state.hasMatched = shouldActivate;
        state.parentActive = parentActive;
        conditionStack_.push(state);
        
        out << "// [Conditional]: #ifndef " << rest 
            << (shouldActivate ? " (active)" : " (inactive)") << "\n";
        return true;
    }
    
    // #if
    if (directive == "if") {
        bool parentActive = isCurrentBranchActive();
        bool condResult = parentActive && evaluateCondition(rest);
        
        ConditionalState state;
        state.active = condResult;
        state.hasMatched = condResult;
        state.parentActive = parentActive;
        conditionStack_.push(state);
        
        out << "// [Conditional]: #if " << rest 
            << (condResult ? " (active)" : " (inactive)") << "\n";
        return true;
    }
    
    // #elif
    if (directive == "elif") {
        if (conditionStack_.empty()) {
            throw std::runtime_error("Preprocessor Error: #elif without #if");
        }
        
        ConditionalState& state = conditionStack_.top();
        
        // 如果之前的分支已经匹配过，这个分支不激活
        if (state.hasMatched) {
            state.active = false;
        } else {
            // 评估条件
            bool condResult = state.parentActive && evaluateCondition(rest);
            state.active = condResult;
            if (condResult) {
                state.hasMatched = true;
            }
        }
        
        out << "// [Conditional]: #elif " << rest 
            << (state.active ? " (active)" : " (inactive)") << "\n";
        return true;
    }
    
    // #else
    if (directive == "else") {
        if (conditionStack_.empty()) {
            throw std::runtime_error("Preprocessor Error: #else without #if");
        }
        
        ConditionalState& state = conditionStack_.top();
        
        // 如果之前的分支没有匹配过，且父级激活，则 #else 激活
        state.active = state.parentActive && !state.hasMatched;
        
        out << "// [Conditional]: #else" 
            << (state.active ? " (active)" : " (inactive)") << "\n";
        return true;
    }
    
    // #endif
    if (directive == "endif") {
        if (conditionStack_.empty()) {
            throw std::runtime_error("Preprocessor Error: #endif without #if");
        }
        
        conditionStack_.pop();
        out << "// [Conditional]: #endif\n";
        return true;
    }
    
    return false;
}

std::string Preprocessor::expandDefined(const std::string& expr) {
    // 处理 defined(MACRO) 和 defined MACRO 形式
    std::string result = expr;
    
    // 处理 defined(MACRO) 形式
    std::regex definedParen(R"(\bdefined\s*\(\s*(\w+)\s*\))");
    std::smatch match;
    std::string temp = result;
    result.clear();
    
    while (std::regex_search(temp, match, definedParen)) {
        result += match.prefix().str();
        std::string macroName = match[1].str();
        result += (macros_.count(macroName) ? "1" : "0");
        temp = match.suffix().str();
    }
    result += temp;
    
    // 处理 defined MACRO (无括号形式)
    std::regex definedNoParen(R"(\bdefined\s+(\w+))");
    temp = result;
    result.clear();
    
    while (std::regex_search(temp, match, definedNoParen)) {
        result += match.prefix().str();
        std::string macroName = match[1].str();
        result += (macros_.count(macroName) ? "1" : "0");
        temp = match.suffix().str();
    }
    result += temp;
    
    return result;
}

/**
 * @brief 求值条件编译表达式
 * 
 * 处理流程：
 * 1. 展开 defined() 操作符
 * 2. 展开所有宏
 * 3. 将未定义的标识符替换为 0（C 标准行为）
 * 4. 递归求值整数表达式
 */
bool Preprocessor::evaluateCondition(const std::string& expr) {
    // 1. 展开 defined() 操作符
    std::string processed = expandDefined(expr);
    
    // 2. 展开宏
    processed = expandMacros(processed);
    
    // 3. 将未定义的标识符替换为 0
    std::string final_expr;
    size_t i = 0;
    while (i < processed.length()) {
        if (std::isalpha(static_cast<unsigned char>(processed[i])) || processed[i] == '_') {
            std::string ident;
            while (i < processed.length() && 
                   (std::isalnum(static_cast<unsigned char>(processed[i])) || processed[i] == '_')) {
                ident += processed[i++];
            }
            final_expr += "0";
        } else {
            final_expr += processed[i++];
        }
    }
    
    // 4. 评估表达式
    try {
        return evaluateExpression(final_expr) != 0;
    } catch (...) {
        return false;
    }
}

/**
 * @brief 递归下降求值整数表达式
 * 
 * 按优先级从低到高处理运算符：
 * || -> && -> ==,!= -> <,>,<=,>= -> +,- -> *,/ -> 一元运算符 -> 括号 -> 数字
 */
long long Preprocessor::evaluateExpression(const std::string& expr) {
    std::string e = trim(expr);
    if (e.empty()) return 0;
    
    // 处理 || (最低优先级)
    int parenDepth = 0;
    for (int i = (int)e.length() - 1; i >= 0; --i) {
        if (e[i] == ')') parenDepth++;
        else if (e[i] == '(') parenDepth--;
        else if (parenDepth == 0 && i > 0 && e[i] == '|' && e[i-1] == '|') {
            return evaluateExpression(e.substr(0, i-1)) || evaluateExpression(e.substr(i+1));
        }
    }
    
    // 处理 &&
    parenDepth = 0;
    for (int i = (int)e.length() - 1; i >= 0; --i) {
        if (e[i] == ')') parenDepth++;
        else if (e[i] == '(') parenDepth--;
        else if (parenDepth == 0 && i > 0 && e[i] == '&' && e[i-1] == '&') {
            return evaluateExpression(e.substr(0, i-1)) && evaluateExpression(e.substr(i+1));
        }
    }
    
    // 处理 ==, !=
    parenDepth = 0;
    for (int i = (int)e.length() - 1; i >= 0; --i) {
        if (e[i] == ')') parenDepth++;
        else if (e[i] == '(') parenDepth--;
        else if (parenDepth == 0 && i > 0) {
            if (e[i] == '=' && e[i-1] == '=') {
                return evaluateExpression(e.substr(0, i-1)) == evaluateExpression(e.substr(i+1));
            }
            if (e[i] == '=' && e[i-1] == '!') {
                return evaluateExpression(e.substr(0, i-1)) != evaluateExpression(e.substr(i+1));
            }
        }
    }
    
    // 处理 <, >, <=, >=
    parenDepth = 0;
    for (int i = (int)e.length() - 1; i >= 0; --i) {
        if (e[i] == ')') parenDepth++;
        else if (e[i] == '(') parenDepth--;
        else if (parenDepth == 0) {
            if (e[i] == '=' && i > 0 && e[i-1] == '<') {
                return evaluateExpression(e.substr(0, i-1)) <= evaluateExpression(e.substr(i+1));
            }
            if (e[i] == '=' && i > 0 && e[i-1] == '>') {
                return evaluateExpression(e.substr(0, i-1)) >= evaluateExpression(e.substr(i+1));
            }
            if (e[i] == '<' && (i == 0 || e[i-1] != '<')) {
                return evaluateExpression(e.substr(0, i)) < evaluateExpression(e.substr(i+1));
            }
            if (e[i] == '>' && (i == 0 || e[i-1] != '>')) {
                return evaluateExpression(e.substr(0, i)) > evaluateExpression(e.substr(i+1));
            }
        }
    }
    
    // 处理 +, - (从右往左找，保证左结合)
    parenDepth = 0;
    for (int i = (int)e.length() - 1; i >= 0; --i) {
        if (e[i] == ')') parenDepth++;
        else if (e[i] == '(') parenDepth--;
        else if (parenDepth == 0 && (e[i] == '+' || e[i] == '-') && i > 0) {
            // 确保不是一元运算符
            char prev = e[i-1];
            if (!std::isspace(static_cast<unsigned char>(prev)) && 
                prev != '(' && prev != '+' && prev != '-' && prev != '*' && prev != '/') {
                if (e[i] == '+') {
                    return evaluateExpression(e.substr(0, i)) + evaluateExpression(e.substr(i+1));
                } else {
                    return evaluateExpression(e.substr(0, i)) - evaluateExpression(e.substr(i+1));
                }
            }
        }
    }
    
    // 处理 *, /
    parenDepth = 0;
    for (int i = (int)e.length() - 1; i >= 0; --i) {
        if (e[i] == ')') parenDepth++;
        else if (e[i] == '(') parenDepth--;
        else if (parenDepth == 0 && (e[i] == '*' || e[i] == '/')) {
            if (e[i] == '*') {
                return evaluateExpression(e.substr(0, i)) * evaluateExpression(e.substr(i+1));
            } else {
                long long rhs = evaluateExpression(e.substr(i+1));
                if (rhs == 0) return 0;
                return evaluateExpression(e.substr(0, i)) / rhs;
            }
        }
    }
    
    // 处理一元 !
    if (!e.empty() && e[0] == '!') {
        return !evaluateExpression(e.substr(1));
    }
    
    // 处理一元 -
    if (!e.empty() && e[0] == '-') {
        return -evaluateExpression(e.substr(1));
    }
    
    // 处理括号
    if (!e.empty() && e[0] == '(' && e.back() == ')') {
        return evaluateExpression(e.substr(1, e.length() - 2));
    }
    
    // 解析数字
    try {
        return std::stoll(e);
    } catch (...) {
        return 0;
    }
}

std::vector<std::string> Preprocessor::parseMacroArgs(const std::string& line, size_t& pos) {
    std::vector<std::string> args;
    while (pos < line.length() && std::isspace(line[pos])) pos++;

    if (pos >= line.length() || line[pos] != '(') return args;
    pos++;

    std::string currentArg;
    int parenDepth = 0;

    while (pos < line.length()) {
        char c = line[pos];
        if (c == '(') {
            parenDepth++;
            currentArg += c;
        } else if (c == ')') {
            if (parenDepth == 0) {
                args.push_back(trim(currentArg));
                pos++;
                return args;
            } else {
                parenDepth--;
                currentArg += c;
            }
        } else if (c == ',' && parenDepth == 0) {
            args.push_back(trim(currentArg));
            currentArg.clear();
        } else {
            currentArg += c;
        }
        pos++;
    }
    throw std::runtime_error("Unclosed macro argument list");
}

/**
 * @brief 替换函数式宏中的参数
 * 
 * 处理流程：
 * 1. 扫描宏体，识别参数名
 * 2. 处理 ## 运算符（Token 连接）
 * 3. 处理 # 运算符（字符串化）
 * 4. 替换参数为实参值
 */
std::string Preprocessor::substituteArgs(const std::string& body,
                                         const std::vector<std::string>& params,
                                         const std::vector<std::string>& args) {
    if (params.size() != args.size()) return body;

    std::string result;
    result.reserve(body.length() * 2);

    // 辅助函数：根据参数名找到参数索引
    auto findParamIndex = [&](const std::string& name) -> int {
        auto it = std::find(params.begin(), params.end(), name);
        if (it != params.end()) {
            return std::distance(params.begin(), it);
        }
        return -1;
    };

    // 辅助函数：将字符串转换为字符串字面量（加双引号和转义）
    auto stringify = [](const std::string& s) -> std::string {
        std::string result = "\"";
        for (char c : s) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else result += c;
        }
        result += "\"";
        return result;
    };

    // 辅助函数：跳过空白字符
    auto skipWhitespace = [&body](size_t& i) {
        while (i < body.length() && std::isspace(static_cast<unsigned char>(body[i]))) {
            i++;
        }
    };

    // 辅助函数：读取标识符
    auto readIdentifier = [&body](size_t& i) -> std::string {
        std::string word;
        while (i < body.length() && 
               (std::isalnum(static_cast<unsigned char>(body[i])) || body[i] == '_')) {
            word += body[i];
            i++;
        }
        return word;
    };

    bool inString = false;
    bool inChar = false;

    for (size_t i = 0; i < body.length(); ) {
        char c = body[i];
        
        // 跟踪字符串和字符字面量状态
        if (!inChar && c == '"') { 
            if (i == 0 || body[i-1] != '\\') inString = !inString; 
        } else if (!inString && c == '\'') { 
            if (i == 0 || body[i-1] != '\\') inChar = !inChar; 
        }

        // 在字符串或字符字面量中，直接复制
        if (inString || inChar) {
            result += c;
            i++;
            continue;
        }

        // 处理 ## (Token 连接运算符)
        if (c == '#' && i + 1 < body.length() && body[i + 1] == '#') {
            // 移除结果末尾的空白
            while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back()))) {
                result.pop_back();
            }
            i += 2; // 跳过 ##
            skipWhitespace(i);
            
            // 读取下一个 token
            if (i < body.length() && (std::isalpha(static_cast<unsigned char>(body[i])) || body[i] == '_')) {
                std::string nextWord = readIdentifier(i);
                int paramIdx = findParamIndex(nextWord);
                if (paramIdx >= 0) {
                    result += args[paramIdx];
                } else {
                    result += nextWord;
                }
            }
            continue;
        }

        // 处理 # (字符串化运算符)
        if (c == '#') {
            i++; // 跳过 #
            skipWhitespace(i);
            
            // 读取参数名
            if (i < body.length() && (std::isalpha(static_cast<unsigned char>(body[i])) || body[i] == '_')) {
                std::string paramName = readIdentifier(i);
                int paramIdx = findParamIndex(paramName);
                if (paramIdx >= 0) {
                    // 字符串化参数
                    result += stringify(args[paramIdx]);
                } else {
                    // 不是参数，保留原样（这是错误用法，但保持向后兼容）
                    result += '#';
                    result += paramName;
                }
            } else {
                // # 后面不是标识符，保留 #
                result += '#';
            }
            continue;
        }

        // 处理标识符（可能是参数）
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string word = readIdentifier(i);
            int paramIdx = findParamIndex(word);
            if (paramIdx >= 0) {
                result += args[paramIdx];
            } else {
                result += word;
            }
            continue;
        }

        // 其他字符直接复制
        result += c;
        i++;
    }

    return result;
}

/**
 * @brief 展开宏
 * 
 * 处理流程：
 * 1. 扫描输入，识别标识符
 * 2. 跳过字符串和字符字面量
 * 3. 对于每个标识符，检查是否为宏
 * 4. 对象式宏：直接替换
 * 5. 函数式宏：解析参数后替换
 * 6. 递归展开，使用 forbidden 集合防止无限递归
 */
std::string Preprocessor::expandMacros(const std::string& line, const std::unordered_set<std::string>& forbidden) {
    if (macros_.empty()) return line;

    std::string result;
    result.reserve(line.length() * 2);

    size_t i = 0;
    while (i < line.length()) {
        char c = line[i];

        // 识别标识符
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string word;
            while (i < line.length() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                word += line[i];
                i++;
            }

            // 1. 检查是否是宏，且不在禁止列表中（防止 #define A A 死循环）
            if (macros_.count(word) && forbidden.find(word) == forbidden.end()) {
                const auto& def = macros_[word];

                std::string substitution;
                bool expanded = false;

                if (def.isFunctionLike) {
                    // 预读 '('
                    size_t checkPos = i;
                    bool hasParen = false;
                    while (checkPos < line.length()) {
                        if (std::isspace(line[checkPos])) checkPos++;
                        else {
                            if (line[checkPos] == '(') hasParen = true;
                            break;
                        }
                    }

                    if (hasParen) {
                        size_t afterCallPos = i; // 从宏名后开始找
                        std::vector<std::string> rawArgs = parseMacroArgs(line, afterCallPos);

                        // 步骤 A: 对实参进行预展开
                        std::vector<std::string> expandedArgs;
                        for (const auto& arg : rawArgs) {
                            // 实参展开时，不继承当前的 forbidden 集合
                            expandedArgs.push_back(expandMacros(arg));
                        }

                        // 步骤 B: 替换文本
                        substitution = substituteArgs(def.body, def.params, expandedArgs);
                        i = afterCallPos; // 更新指针，跳过整个宏调用
                        expanded = true;
                    }
                } else {
                    // 对象式宏，直接替换
                    substitution = def.body;
                    expanded = true;
                }

                if (expanded) {
                    // 步骤 C: 对替换后的结果进行重扫描 (Rescan)
                    // 将当前宏加入 forbidden，防止直接递归
                    std::unordered_set<std::string> nextForbidden = forbidden;
                    nextForbidden.insert(word);

                    // 递归展开结果字符串
                    result += expandMacros(substitution, nextForbidden);
                } else {
                    // 函数式宏但没带括号，不展开
                    result += word;
                }
            } else {
                // 不是宏，或已被禁止展开
                result += word;
            }
        } else {
            // 非标识符，直接追加
            result += c;
            i++;
        }
    }
    return result;
}

std::string Preprocessor::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}