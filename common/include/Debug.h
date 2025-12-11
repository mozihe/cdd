/**
 * @file Debug.h
 * @brief 调试辅助宏
 * @author mozihe
 * 
 * 提供编译器调试输出功能。
 * 使用 cmake -DCDD_ENABLE_DEBUG=ON 编译时启用调试输出。
 */

#ifndef CDD_DEBUG_H
#define CDD_DEBUG_H

#include <iostream>
#include <string>

#ifdef CDD_DEBUG

/**
 * @brief 调试输出宏
 * @param msg 要输出的消息
 */
#define CDD_DBG(msg) \
    std::cerr << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " - " << msg << std::endl

/**
 * @brief 调试输出带变量
 * @param var 变量名和值
 */
#define CDD_DBG_VAR(var) \
    std::cerr << "[DEBUG] " << #var << " = " << (var) << std::endl

/**
 * @brief 进入函数调试
 * @param func 函数名
 */
#define CDD_DBG_ENTER(func) \
    std::cerr << "[DEBUG] >>> Entering " << func << std::endl

/**
 * @brief 离开函数调试
 * @param func 函数名
 */
#define CDD_DBG_EXIT(func) \
    std::cerr << "[DEBUG] <<< Exiting " << func << std::endl

/**
 * @brief 阶段开始
 * @param stage 阶段名
 */
#define CDD_DBG_STAGE(stage) \
    std::cerr << "[DEBUG] ========== " << stage << " ==========" << std::endl

#else

// 非调试模式下，这些宏不产生任何代码
#define CDD_DBG(msg) ((void)0)
#define CDD_DBG_VAR(var) ((void)0)
#define CDD_DBG_ENTER(func) ((void)0)
#define CDD_DBG_EXIT(func) ((void)0)
#define CDD_DBG_STAGE(stage) ((void)0)

#endif // CDD_DEBUG

#endif // CDD_DEBUG_H
