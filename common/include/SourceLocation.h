/**
 * @file SourceLocation.h
 * @brief 源码位置信息
 * @author mozihe
 * 
 * 定义源代码位置结构，用于错误报告和调试信息生成。
 */

#ifndef CDD_SOURCE_LOCATION_H
#define CDD_SOURCE_LOCATION_H

#include <cstdint>
#include <string>
#include <string_view>

namespace cdd {

/**
 * @struct SourceLocation
 * @brief 源代码位置
 *
 * 记录源代码中的精确位置，包括文件名、行号、列号。
 */
struct SourceLocation {
    std::string_view filename;  ///< 文件名
    uint32_t line;              ///< 行号（从 1 开始）
    uint32_t column;            ///< 列号（从 1 开始）
    uint32_t offset;            ///< 文件内字节偏移

    SourceLocation() : line(0), column(0), offset(0) {}

    SourceLocation(std::string_view file, uint32_t l, uint32_t c, uint32_t off = 0)
        : filename(file), line(l), column(c), offset(off) {}

    /// @brief 是否为有效位置
    bool isValid() const { return line > 0; }

    /// @brief 格式化为字符串 "file:line:column"
    std::string toString() const {
        if (!isValid()) return "<unknown>";
        std::string result;
        if (!filename.empty()) {
            result += filename;
            result += ':';
        }
        result += std::to_string(line);
        result += ':';
        result += std::to_string(column);
        return result;
    }
};

/**
 * @struct SourceRange
 * @brief 源代码范围
 *
 * 表示源代码中的一段区域（从开始到结束）。
 */
struct SourceRange {
    SourceLocation begin;   ///< 起始位置
    SourceLocation end;     ///< 结束位置

    SourceRange() = default;

    SourceRange(const SourceLocation& b, const SourceLocation& e)
        : begin(b), end(e) {}

    explicit SourceRange(const SourceLocation& loc)
        : begin(loc), end(loc) {}

    bool isValid() const { return begin.isValid(); }
};

} // namespace cdd

#endif // CDD_SOURCE_LOCATION_H
