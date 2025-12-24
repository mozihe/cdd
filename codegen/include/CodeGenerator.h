/**
 * @file CodeGenerator.h
 * @brief x86-64 代码生成器
 * @author mozihe
 * 
 * 将四元式中间代码转换为 x86-64 汇编代码。
 * 目标平台：Linux x86-64，遵循 System V AMD64 ABI。
 */

#ifndef CDD_CODEGEN_CODE_GENERATOR_H
#define CDD_CODEGEN_CODE_GENERATOR_H

#include "IRGenerator.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

namespace cdd {
namespace codegen {

/**
 * @enum Register
 * @brief x86-64 通用寄存器枚举
 */
enum class Register {
    RAX, RBX, RCX, RDX,
    RSI, RDI, RBP, RSP,
    R8, R9, R10, R11,
    R12, R13, R14, R15,
    NONE
};

/**
 * @enum XmmRegister
 * @brief SSE/AVX 浮点寄存器枚举
 * 
 * 用于浮点运算和 SIMD 操作，遵循 System V AMD64 ABI：
 * - XMM0-XMM7：传递浮点参数
 * - XMM0-XMM1：返回浮点值
 * - XMM8-XMM15：调用者保存寄存器
 */
enum class XmmRegister {
    XMM0, XMM1, XMM2, XMM3,
    XMM4, XMM5, XMM6, XMM7,
    XMM8, XMM9, XMM10, XMM11,
    XMM12, XMM13, XMM14, XMM15,
    NONE
};

/**
 * @brief 获取寄存器名称字符串
 * @param reg 寄存器枚举值
 * @param size 寄存器大小（1=8位, 2=16位, 4=32位, 8=64位）
 * @return AT&T 语法的寄存器名（如 %rax, %eax, %ax, %al）
 */
std::string regName(Register reg, int size = 8);

/**
 * @brief 获取 XMM 寄存器名称字符串
 * @param reg XMM 寄存器枚举值
 * @return AT&T 语法的寄存器名（如 %xmm0）
 */
std::string xmmName(XmmRegister reg);

/**
 * @struct Location
 * @brief 变量或临时值的存储位置描述
 * 
 * 描述一个值在运行时的存储位置，可以是寄存器、栈、全局内存或立即数。
 */
struct Location {
    enum class Type {
        Register,   ///< 在寄存器中
        Stack,      ///< 在栈上（相对 RBP 的偏移）
        Global,     ///< 全局变量
        Immediate   ///< 立即数
    };
    
    Type type;
    Register reg = Register::NONE;
    int offset = 0;
    std::string name;
    
    Location() : type(Type::Stack), offset(0) {}
    Location(Register r) : type(Type::Register), reg(r) {}
    Location(int off) : type(Type::Stack), offset(off) {}
    Location(const std::string& n, bool isImm = false) 
        : type(isImm ? Type::Immediate : Type::Global), name(n) {}
    
    /**
     * @brief 转换为 AT&T 汇编语法的操作数字符串
     * @param size 操作数大小（字节）
     * @return 汇编操作数字符串（如 -8(%rbp), %rax, $42）
     */
    std::string toAsm(int size = 8) const;
};

/**
 * @class RegisterAllocator
 * @brief 简单线性扫描寄存器分配器
 * 
 * 管理通用寄存器的分配和释放，跟踪哪些寄存器可用。
 */
class RegisterAllocator {
public:
    RegisterAllocator();
    
    /**
     * @brief 分配一个可用寄存器
     * @return 分配的寄存器，如无可用则返回 NONE
     */
    Register allocate();
    
    /**
     * @brief 释放寄存器
     * @param reg 要释放的寄存器
     */
    void release(Register reg);
    
    /**
     * @brief 检查寄存器是否可用
     * @param reg 要检查的寄存器
     * @return 如果可用返回 true
     */
    bool isAvailable(Register reg) const;
    
    /**
     * @brief 尝试分配指定寄存器
     * @param reg 要分配的特定寄存器
     * @return 如果成功返回 true
     */
    bool allocateSpecific(Register reg);
    
    /**
     * @brief 释放所有调用者保存寄存器
     */
    void releaseCallerSaved();
    
    /**
     * @brief 获取已使用的被调用者保存寄存器列表
     * @return 已使用的 RBX, R12-R15 寄存器列表
     */
    std::vector<Register> getUsedCalleeSaved() const;
    
private:
    std::unordered_set<Register> available_;
    std::unordered_set<Register> used_;
};

/**
 * @class XmmAllocator
 * @brief XMM 浮点寄存器分配器
 * 
 * 管理 XMM 寄存器的分配和释放，用于浮点运算。
 * 遵循 System V AMD64 ABI 调用约定。
 */
class XmmAllocator {
public:
    XmmAllocator();
    
    /**
     * @brief 分配一个可用的 XMM 寄存器
     * @return 分配的寄存器，如无可用则返回 NONE
     */
    XmmRegister allocate();
    
    /**
     * @brief 释放 XMM 寄存器
     * @param reg 要释放的寄存器
     */
    void release(XmmRegister reg);
    
    /**
     * @brief 检查 XMM 寄存器是否可用
     * @param reg 要检查的寄存器
     * @return 如果可用返回 true
     */
    bool isAvailable(XmmRegister reg) const;
    
    /**
     * @brief 尝试分配指定的 XMM 寄存器
     * @param reg 要分配的特定寄存器
     * @return 如果成功返回 true
     */
    bool allocateSpecific(XmmRegister reg);
    
    /**
     * @brief 释放所有 XMM 寄存器（均为调用者保存）
     */
    void releaseAll();
    
private:
    std::unordered_set<XmmRegister> available_;
    std::unordered_set<XmmRegister> used_;
};

/**
 * @class CodeGenerator
 * @brief x86-64 汇编代码生成器
 * 
 * 将 IR 四元式序列转换为 AT&T 语法的 x86-64 汇编代码。
 * 
 * 主要功能：
 * 1. 生成数据段（全局变量、字符串字面量）
 * 2. 生成代码段（函数体）
 * 3. 处理函数调用约定（参数传递、返回值）
 * 4. 管理栈帧布局
 */
class CodeGenerator {
public:
    /**
     * @brief 构造代码生成器
     * @param program 要翻译的 IR 程序
     */
    explicit CodeGenerator(const semantic::IRProgram& program);
    
    /**
     * @brief 生成完整的汇编代码
     * @return 汇编代码字符串（包含所有段）
     */
    std::string generate();
    
    /**
     * @brief 将汇编代码写入文件
     * @param filename 输出文件路径（.s 扩展名）
     * @return 写入成功返回 true
     */
    bool writeToFile(const std::string& filename);
    
private:
    const semantic::IRProgram& program_;
    RegisterAllocator regAlloc_;
    XmmAllocator xmmAlloc_;        ///< XMM 浮点寄存器分配器
    
    std::stringstream dataSection_;
    std::stringstream bssSection_;
    std::stringstream rodataSection_;
    std::stringstream textSection_;
    
    std::string currentFunction_;
    int stackSize_ = 0;
    int localOffset_ = 0;
    int minLocalOffset_ = 0;              ///< 记录当前函数的最小栈偏移，用于最终栈大小计算
    bool inFunction_ = false;
    std::string currentStackPlaceholder_; ///< 当前函数序言中的栈大小占位符
    
    std::unordered_map<std::string, Location> locations_;
    std::vector<semantic::Operand> callParams_;
    std::vector<int> raxSpillStack_;        ///< 记录 RAX 回退的嵌套信息
    std::vector<int> freeRaxSpillSlots_;    ///< 可复用的 RAX 溢出槽位
    
    std::unordered_map<double, std::string> floatLiterals_;  ///< 浮点数常量标签映射
    int floatLabelCounter_ = 0;
    
    /** @brief 获取或创建浮点数常量标签 */
    std::string getFloatLabel(double value);
    
    /** @brief 生成浮点数字面量到 .rodata 段 */
    void emitFloatLiterals();
    
    /** @brief 将序言中的栈大小占位符替换为真实大小 */
    void finalizeStackSize(const std::string& funcName, int stackSize);

    /** @brief 发射一行汇编指令 */
    void emitLine(const std::string& line);
    
    /** @brief 发射标签定义 */
    void emitLabel(const std::string& label);
    
    /** @brief 发射注释 */
    void emitComment(const std::string& comment);
    
    /** @brief 发射汇编伪指令 */
    void emitDirective(const std::string& directive);
    
    /** @brief 生成全局变量定义 */
    void emitGlobals();
    
    /** @brief 生成字符串字面量到 .rodata 段 */
    void emitStringLiterals();
    
    /**
     * @brief 生成函数代码
     * @param func 函数 IR
     */
    void emitFunction(const semantic::FunctionIR& func);
    
    /**
     * @brief 生成函数序言（保存寄存器、分配栈帧）
     * @param func 函数 IR
     */
    void emitPrologue(const semantic::FunctionIR& func);
    
    /** @brief 生成函数尾声（恢复寄存器、返回） */
    void emitEpilogue();
    
    /**
     * @brief 获取操作数的存储位置
     * @param operand IR 操作数
     * @return 存储位置描述
     */
    Location getLocation(const semantic::Operand& operand);
    
    /**
     * @brief 将操作数转换为汇编字符串
     * @param operand IR 操作数
     * @param size 操作数大小（字节）
     * @return 汇编操作数字符串
     */
    std::string operandToAsm(const semantic::Operand& operand, int size = 8);
    
    /**
     * @brief 将操作数加载到寄存器
     * @param operand IR 操作数
     * @return 加载到的寄存器
     */
    Register loadToRegister(const semantic::Operand& operand);
    
    /**
     * @brief 将寄存器值存储到目标操作数
     * @param reg 源寄存器
     * @param dest 目标操作数
     */
    void storeFromRegister(Register reg, const semantic::Operand& dest);
    
    /** @brief 分配一个通用寄存器，必要时回退到 RAX */
    Register acquireRegister();
    
    /** @brief 释放通用寄存器，处理 RAX 回退恢复 */
    void releaseRegister(Register reg);
    
    /** @brief 获取或创建一个 RAX 溢出槽位 */
    int acquireRaxSpillSlot();
    
    /**
     * @brief 翻译单条四元式
     * @param quad 四元式
     */
    void translateQuad(const semantic::Quadruple& quad);
    
    /** @brief 翻译整数算术运算（Add, Sub, Mul, Div, Mod） */
    void translateArithmetic(const semantic::Quadruple& quad);
    
    /** @brief 翻译整数取负运算 */
    void translateNeg(const semantic::Quadruple& quad);
    
    /** @brief 翻译浮点算术运算（FAdd, FSub, FMul, FDiv） */
    void translateFloatArithmetic(const semantic::Quadruple& quad);
    
    /** @brief 翻译浮点取负运算 */
    void translateFloatNeg(const semantic::Quadruple& quad);
    
    /** @brief 翻译位运算（And, Or, Xor, Shl, Shr） */
    void translateBitwise(const semantic::Quadruple& quad);
    
    /** @brief 翻译按位取反 */
    void translateBitNot(const semantic::Quadruple& quad);
    
    /** @brief 翻译整数比较运算（Eq, Ne, Lt, Le, Gt, Ge） */
    void translateComparison(const semantic::Quadruple& quad);
    
    /** @brief 翻译浮点比较运算（FEq, FNe, FLt, FLe, FGt, FGe） */
    void translateFloatComparison(const semantic::Quadruple& quad);
    
    /** @brief 翻译逻辑运算（And, Or） */
    void translateLogical(const semantic::Quadruple& quad);
    
    /** @brief 翻译逻辑非 */
    void translateLogNot(const semantic::Quadruple& quad);
    
    /** @brief 翻译赋值 */
    void translateAssign(const semantic::Quadruple& quad);
    
    /** @brief 翻译内存加载（解引用） */
    void translateLoad(const semantic::Quadruple& quad);
    
    /** @brief 翻译内存存储 */
    void translateStore(const semantic::Quadruple& quad);
    
    /** @brief 翻译取地址 */
    void translateLoadAddr(const semantic::Quadruple& quad);
    
    /** @brief 翻译数组索引地址计算 */
    void translateIndexAddr(const semantic::Quadruple& quad);
    
    /** @brief 翻译结构体成员地址计算 */
    void translateMemberAddr(const semantic::Quadruple& quad);
    
    /** @brief 翻译类型转换 */
    void translateCast(const semantic::Quadruple& quad);
    
    /** @brief 翻译无条件跳转 */
    void translateJump(const semantic::Quadruple& quad);
    
    /** @brief 翻译条件为真跳转 */
    void translateJumpTrue(const semantic::Quadruple& quad);
    
    /** @brief 翻译条件为假跳转 */
    void translateJumpFalse(const semantic::Quadruple& quad);
    
    /** @brief 翻译标签定义 */
    void translateLabel(const semantic::Quadruple& quad);
    
    /** @brief 翻译函数参数传递 */
    void translateParam(const semantic::Quadruple& quad);
    
    /** @brief 翻译函数调用 */
    void translateCall(const semantic::Quadruple& quad);
    
    /** @brief 翻译函数返回 */
    void translateReturn(const semantic::Quadruple& quad);
    
    /**
     * @brief 在栈上分配空间
     * @param size 需要的字节数
     * @param align 对齐要求
     * @return 分配位置的栈偏移（负值）
     */
    int allocateStack(int size, int align = 8);
    
    /**
     * @brief 对齐栈指针
     * @param align 对齐边界（字节）
     */
    void alignStack(int align = 16);
    
    /**
     * @brief 获取类型大小
     * @param type 语义类型
     * @return 类型大小（字节）
     */
    int getTypeSize(semantic::TypePtr type) const;
    
    /**
     * @brief 获取指令大小后缀
     * @param size 操作数大小（字节）
     * @return 后缀字符串（b/w/l/q）
     */
    std::string getSizeSuffix(int size) const;
    
    /**
     * @brief 判断操作数是否为浮点类型
     * @param operand 操作数
     * @return 如果是浮点类型返回 true
     */
    bool isFloatType(const semantic::Operand& operand) const;
    
    /**
     * @brief 将浮点操作数加载到 XMM 寄存器
     * @param operand 操作数
     * @return 分配的 XMM 寄存器
     */
    XmmRegister loadToXmm(const semantic::Operand& operand);
    
    /**
     * @brief 将 XMM 寄存器值存储到目标操作数
     * @param reg XMM 寄存器
     * @param dest 目标操作数
     */
    void storeFromXmm(XmmRegister reg, const semantic::Operand& dest);
};

/**
 * @brief 汇编并链接生成可执行文件
 * @param asmFile 汇编源文件路径
 * @param outputFile 输出可执行文件路径
 * @return 成功返回 true
 * 
 * 调用 GNU as 进行汇编，然后调用 gcc 进行链接。
 * 自动链接 CDD 标准库 libcdd.so。
 */
bool assembleAndLink(const std::string& asmFile, const std::string& outputFile);

} // namespace codegen
} // namespace cdd

#endif // CDD_CODEGEN_CODE_GENERATOR_H
