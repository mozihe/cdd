/**
 * @file Preprocessor.h
 * @brief C 预处理器
 * @author mozihe
 * 
 * 实现 C 语言预处理功能：
 * - #include 文件包含
 * - #define / #undef 宏定义
 * - #if / #ifdef / #ifndef / #elif / #else / #endif 条件编译
 * - 宏展开（含 # 字符串化和 ## 连接运算符）
 */

#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <string>
#include <vector>
#include <stack>
#include <unordered_set>
#include <unordered_map>
#include <sstream>

/**
 * @struct MacroDef
 * @brief 宏定义结构
 */
struct MacroDef {
    bool isFunctionLike = false;      ///< 是否为函数式宏
    std::vector<std::string> params;  ///< 参数列表（函数式宏）
    std::string body;                 ///< 宏体
};

/**
 * @struct ConditionalState
 * @brief 条件编译栈帧状态
 */
struct ConditionalState {
    bool active;        ///< 当前分支是否激活
    bool hasMatched;    ///< 是否已有分支匹配（用于 #elif/#else）
    bool parentActive;  ///< 父级条件是否激活
};

/**
 * @class Preprocessor
 * @brief C 预处理器
 * 
 * 处理预处理指令，输出展开后的代码供词法分析器处理。
 */
class Preprocessor {
public:
    /**
     * @brief 构造预处理器
     * 
     * 自动从环境变量读取包含路径：
     * - CDD_INCLUDE_PATH: 用户头文件路径
     * - CDD_STDLIB_PATH: 标准库路径
     */
    Preprocessor();
    
    /**
     * @brief 预处理源文件
     * @param filename 源文件路径
     * @return 预处理后的完整代码
     * @throws std::runtime_error 文件不存在或条件编译未闭合
     */
    std::string preprocess(const std::string& filename);
    
    /**
     * @brief 添加头文件搜索路径
     * @param path 目录路径
     */
    void addIncludePath(const std::string& path);
    
    /**
     * @brief 预定义宏
     * @param name 宏名称
     * @param value 宏值（默认 "1"）
     */
    void defineMacro(const std::string& name, const std::string& value = "1");
    
    /**
     * @brief 获取所有包含路径
     * @return 路径列表的常量引用
     */
    const std::vector<std::string>& includePaths() const { return includePaths_; }
    

private:
    std::unordered_set<std::string> loadedFiles_;
    std::unordered_map<std::string, MacroDef> macros_;
    std::vector<std::string> includePaths_;
    std::string currentDir_;
    std::stack<ConditionalState> conditionStack_;

    /**
     * @brief 初始化包含路径
     * 
     * 从环境变量和默认位置加载搜索路径。
     */
    void initIncludePaths();
    
    /**
     * @brief 处理单个文件
     * @param filename 文件路径
     * @param out 输出流
     */
    void processFile(const std::string& filename, std::stringstream& out);
    
    /**
     * @brief 解析 #include 指令
     * @param line 源代码行
     * @return {是否为include, {头文件名, 是否系统头文件}}
     */
    std::pair<bool, std::pair<std::string, bool>> parseInclude(const std::string& line);
    
    /**
     * @brief 解析头文件路径
     * @param headerName 头文件名
     * @param isSystemHeader 是否为系统头文件（<> vs ""）
     * @return 解析后的完整路径，失败返回空串
     */
    std::string resolveIncludePath(const std::string& headerName, bool isSystemHeader);
    
    /**
     * @brief 解析 #define 指令
     * @param line 源代码行
     * @return 解析成功返回 true
     */
    bool parseDefine(const std::string& line);
    
    /**
     * @brief 解析 #undef 指令
     * @param line 源代码行
     * @return 解析成功返回 true
     */
    bool parseUndef(const std::string& line);

    /**
     * @brief 检查当前条件分支是否激活
     * @return 代码应被包含返回 true
     */
    bool isCurrentBranchActive() const;
    
    /**
     * @brief 处理条件编译指令
     * @param line 源代码行
     * @param out 输出流
     * @return 该行是条件指令返回 true
     */
    bool handleConditional(const std::string& line, std::stringstream& out);
    
    /**
     * @brief 求值条件表达式
     * @param expr 表达式字符串
     * @return 表达式为真返回 true
     */
    bool evaluateCondition(const std::string& expr);
    
    /**
     * @brief 求值整数表达式
     * @param expr 表达式字符串
     * @return 表达式的整数值
     */
    long long evaluateExpression(const std::string& expr);
    
    /**
     * @brief 展开 defined() 运算符
     * @param expr 包含 defined 的表达式
     * @return 展开后的表达式
     */
    std::string expandDefined(const std::string& expr);

    /**
     * @brief 展开宏
     * @param line 输入文本
     * @param forbidden 禁止展开的宏集合（防止无限递归）
     * @return 宏展开后的文本
     */
    std::string expandMacros(const std::string& line, 
                             const std::unordered_set<std::string>& forbidden = {});

    /**
     * @brief 解析函数式宏参数
     * @param line 源文本
     * @param pos 起始位置（会被更新）
     * @return 参数列表
     */
    std::vector<std::string> parseMacroArgs(const std::string& line, size_t& pos);
    
    /**
     * @brief 替换宏参数
     * @param body 宏体
     * @param params 形参列表
     * @param args 实参列表
     * @return 替换后的文本
     */
    std::string substituteArgs(const std::string& body,
                               const std::vector<std::string>& params,
                               const std::vector<std::string>& args);
    
    /**
     * @brief 去除字符串首尾空白
     * @param str 输入字符串
     * @return 去除空白后的字符串
     */
    std::string trim(const std::string& str);
};

#endif // PREPROCESSOR_H
