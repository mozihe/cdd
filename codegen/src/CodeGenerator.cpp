/**
 * @file CodeGenerator.cpp
 * @brief x86-64 代码生成器实现
 * @author mozihe
 */

#include "CodeGenerator.h"
#include <fstream>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace cdd {
namespace codegen {

static const char* regNames64[] = {
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", ""
};

static const char* regNames32[] = {
    "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d", ""
};

static const char* regNames16[] = {
    "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w", ""
};

static const char* regNames8[] = {
    "al", "bl", "cl", "dl", "sil", "dil", "bpl", "spl",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b", ""
};

std::string regName(Register reg, int size) {
    int idx = static_cast<int>(reg);
    if (idx < 0 || idx >= 16) return "";
    
    switch (size) {
        case 1: return std::string("%") + regNames8[idx];
        case 2: return std::string("%") + regNames16[idx];
        case 4: return std::string("%") + regNames32[idx];
        default: return std::string("%") + regNames64[idx];
    }
}

static const char* xmmRegNames[] = {
    "xmm0", "xmm1", "xmm2", "xmm3",
    "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11",
    "xmm12", "xmm13", "xmm14", "xmm15", ""
};

std::string xmmName(XmmRegister reg) {
    int idx = static_cast<int>(reg);
    if (idx < 0 || idx >= 16) return "";
    return std::string("%") + xmmRegNames[idx];
}

// ============================================================================
// Location
// ============================================================================

std::string Location::toAsm(int size) const {
    (void)size;
    switch (type) {
        case Type::Register:
            return regName(reg, 8);
        case Type::Stack:
            return std::to_string(offset) + "(%rbp)";
        case Type::Global:
            return name + "(%rip)";
        case Type::Immediate:
            return "$" + name;
    }
    return "";
}

// ============================================================================
// RegisterAllocator
// ============================================================================

RegisterAllocator::RegisterAllocator() {
    available_ = {
        Register::RBX, Register::RCX, Register::RDX,
        Register::RSI, Register::RDI,
        Register::R8, Register::R9, Register::R10, Register::R11,
        Register::R12, Register::R13, Register::R14, Register::R15
    };
}

Register RegisterAllocator::allocate() {
    if (available_.empty()) return Register::NONE;
    
    static const Register preferred[] = {
        Register::R10, Register::R11, Register::RCX, Register::RDX,
        Register::RSI, Register::RDI, Register::R8, Register::R9
    };
    
    for (auto reg : preferred) {
        if (available_.count(reg)) {
            available_.erase(reg);
            used_.insert(reg);
            return reg;
        }
    }
    
    auto it = available_.begin();
    Register reg = *it;
    available_.erase(it);
    used_.insert(reg);
    return reg;
}

void RegisterAllocator::release(Register reg) {
    if (reg != Register::NONE && reg != Register::RAX && 
        reg != Register::RSP && reg != Register::RBP) {
        used_.erase(reg);
        available_.insert(reg);
    }
}

bool RegisterAllocator::isAvailable(Register reg) const {
    return available_.count(reg) > 0;
}

bool RegisterAllocator::allocateSpecific(Register reg) {
    if (available_.count(reg)) {
        available_.erase(reg);
        used_.insert(reg);
        return true;
    }
    return false;
}

void RegisterAllocator::releaseCallerSaved() {
    static const Register callerSaved[] = {
        Register::RAX, Register::RCX, Register::RDX,
        Register::RSI, Register::RDI,
        Register::R8, Register::R9, Register::R10, Register::R11
    };
    for (auto reg : callerSaved) {
        if (used_.count(reg)) {
            used_.erase(reg);
            available_.insert(reg);
        }
    }
}

std::vector<Register> RegisterAllocator::getUsedCalleeSaved() const {
    std::vector<Register> result;
    static const Register calleeSaved[] = {
        Register::RBX, Register::R12, Register::R13, Register::R14, Register::R15
    };
    for (auto reg : calleeSaved) {
        if (used_.count(reg)) {
            result.push_back(reg);
        }
    }
    return result;
}

// ============================================================================
// XmmAllocator
// ============================================================================

XmmAllocator::XmmAllocator() {
    // XMM8-XMM15 优先使用（避免与参数寄存器冲突）
    available_ = {
        XmmRegister::XMM8, XmmRegister::XMM9,
        XmmRegister::XMM10, XmmRegister::XMM11,
        XmmRegister::XMM12, XmmRegister::XMM13,
        XmmRegister::XMM14, XmmRegister::XMM15,
        XmmRegister::XMM2, XmmRegister::XMM3,
        XmmRegister::XMM4, XmmRegister::XMM5,
        XmmRegister::XMM6, XmmRegister::XMM7
    };
}

XmmRegister XmmAllocator::allocate() {
    if (available_.empty()) return XmmRegister::NONE;
    
    // 优先使用 XMM8-XMM15，避免与参数传递冲突
    static const XmmRegister preferred[] = {
        XmmRegister::XMM8, XmmRegister::XMM9,
        XmmRegister::XMM10, XmmRegister::XMM11,
        XmmRegister::XMM12, XmmRegister::XMM13,
        XmmRegister::XMM14, XmmRegister::XMM15
    };
    
    for (auto reg : preferred) {
        if (available_.count(reg)) {
            available_.erase(reg);
            used_.insert(reg);
            return reg;
        }
    }
    
    auto it = available_.begin();
    XmmRegister reg = *it;
    available_.erase(it);
    used_.insert(reg);
    return reg;
}

void XmmAllocator::release(XmmRegister reg) {
    if (reg != XmmRegister::NONE) {
        used_.erase(reg);
        available_.insert(reg);
    }
}

bool XmmAllocator::isAvailable(XmmRegister reg) const {
    return available_.count(reg) > 0;
}

bool XmmAllocator::allocateSpecific(XmmRegister reg) {
    if (available_.count(reg)) {
        available_.erase(reg);
        used_.insert(reg);
        return true;
    }
    return false;
}

void XmmAllocator::releaseAll() {
    for (auto reg : used_) {
        available_.insert(reg);
    }
    used_.clear();
}

// ============================================================================
// CodeGenerator
// ============================================================================

CodeGenerator::CodeGenerator(const semantic::IRProgram& program)
    : program_(program) {}

/**
 * @brief 生成完整的汇编代码
 * 
 * 处理流程：
 * 1. 清空所有段缓冲区
 * 2. 生成 .rodata 段（字符串字面量）
 * 3. 生成 .bss 段（全局变量）
 * 4. 生成 .text 段（函数代码）
 * 5. 组装所有段为最终输出
 */
std::string CodeGenerator::generate() {
    // 1. 清空所有段缓冲区
    dataSection_.str("");
    bssSection_.str("");
    rodataSection_.str("");
    textSection_.str("");
    
    // 2. 生成字符串字面量到 .rodata 段
    emitStringLiterals();
    
    // 3. 生成全局变量到 .bss 段
    emitGlobals();
    
    // 4. 生成所有函数到 .text 段
    for (const auto& func : program_.functions) {
        emitFunction(func);
    }
    
    // 5. 生成浮点数字面量到 .rodata 段（在函数翻译后，因为那时才收集到浮点常量）
    emitFloatLiterals();
    
    // 6. 组装最终输出（按 ELF 段顺序）
    std::stringstream output;
    
    output << "# Generated by CDD Compiler\n";
    output << "    .file \"output.s\"\n\n";
    
    if (rodataSection_.tellp() > 0) {
        output << "    .section .rodata\n";
        output << rodataSection_.str();
        output << "\n";
    }
    
    if (dataSection_.tellp() > 0) {
        output << "    .data\n";
        output << dataSection_.str();
        output << "\n";
    }
    
    if (bssSection_.tellp() > 0) {
        output << "    .bss\n";
        output << bssSection_.str();
        output << "\n";
    }
    
    output << "    .text\n";
    output << textSection_.str();
    
    // 添加 .note.GNU-stack 节，标记栈不可执行（消除链接器警告）
    output << "\n    .section .note.GNU-stack,\"\",@progbits\n";
    
    return output.str();
}

bool CodeGenerator::writeToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file) return false;
    file << generate();
    return file.good();
}

// ============================================================================
// 代码发射
// ============================================================================

void CodeGenerator::emitLine(const std::string& line) {
    textSection_ << "    " << line << "\n";
}

void CodeGenerator::emitLabel(const std::string& label) {
    textSection_ << label << ":\n";
}

void CodeGenerator::emitComment(const std::string& comment) {
    textSection_ << "    # " << comment << "\n";
}

void CodeGenerator::emitDirective(const std::string& directive) {
    textSection_ << "    " << directive << "\n";
}

// ============================================================================
// 全局数据
// ============================================================================

void CodeGenerator::emitStringLiterals() {
    for (const auto& [label, value] : program_.stringLiterals) {
        rodataSection_ << label << ":\n";
        rodataSection_ << "    .string \"";
        // 转义特殊字符
        for (char c : value) {
            switch (c) {
                case '\n': rodataSection_ << "\\n"; break;
                case '\t': rodataSection_ << "\\t"; break;
                case '\r': rodataSection_ << "\\r"; break;
                case '\\': rodataSection_ << "\\\\"; break;
                case '"':  rodataSection_ << "\\\""; break;
                case '\0': rodataSection_ << "\\0"; break;
                default:
                    if (c >= 32 && c < 127) {
                        rodataSection_ << c;
                    } else {
                        // 其他不可打印字符用八进制
                        rodataSection_ << "\\" << std::oct << (int)(unsigned char)c << std::dec;
                    }
            }
        }
        rodataSection_ << "\"\n";
    }
}

std::string CodeGenerator::getFloatLabel(double value) {
    auto it = floatLiterals_.find(value);
    if (it != floatLiterals_.end()) {
        return it->second;
    }
    std::string label = ".LF" + std::to_string(floatLabelCounter_++);
    floatLiterals_[value] = label;
    return label;
}

void CodeGenerator::emitFloatLiterals() {
    for (const auto& [value, label] : floatLiterals_) {
        rodataSection_ << "    .align 8\n";
        rodataSection_ << label << ":\n";
        union { double d; uint64_t u; } conv;
        conv.d = value;
        rodataSection_ << "    .quad " << conv.u << "\n";
    }
}

void CodeGenerator::emitGlobals() {
    for (const auto& global : program_.globals) {
        if (global.isExtern) continue;  // extern 声明不生成定义
        
        int size = getTypeSize(global.type);
        
        if (global.hasInitializer && !global.initValues.empty()) {
            // 有初始化值，放到 .data 段
            dataSection_ << "    .globl " << global.name << "\n";
            dataSection_ << "    .align " << std::min(size, 8) << "\n";
            dataSection_ << global.name << ":\n";
            
            for (const auto& val : global.initValues) {
                using Kind = semantic::GlobalInitValue::Kind;
                switch (val.kind) {
                    case Kind::Integer:
                        switch (val.size) {
                            case 1: dataSection_ << "    .byte " << val.intValue << "\n"; break;
                            case 2: dataSection_ << "    .short " << val.intValue << "\n"; break;
                            case 4: dataSection_ << "    .long " << val.intValue << "\n"; break;
                            default: dataSection_ << "    .quad " << val.intValue << "\n"; break;
                        }
                        break;
                    case Kind::Float: {
                        union { double d; uint64_t u; } conv;
                        conv.d = val.floatValue;
                        if (val.size == 4) {
                            // float 类型：转换为 32 位表示
                            union { float f; uint32_t u; } fconv;
                            fconv.f = static_cast<float>(val.floatValue);
                            dataSection_ << "    .long " << fconv.u << "\n";
                        } else {
                            dataSection_ << "    .quad " << conv.u << "\n";
                        }
                        break;
                    }
                    case Kind::String:
                        dataSection_ << "    .quad " << val.strLabel << "\n";
                        break;
                    case Kind::Address:
                        dataSection_ << "    .quad " << val.strLabel << "\n";
                        break;
                    case Kind::Zero:
                        dataSection_ << "    .zero " << val.size << "\n";
                        break;
                }
            }
        } else {
            // 无初始化值，放到 .bss 段
            bssSection_ << "    .globl " << global.name << "\n";
            bssSection_ << "    .align " << std::min(size, 8) << "\n";
            bssSection_ << global.name << ":\n";
            bssSection_ << "    .zero " << size << "\n";
        }
    }
}

// ============================================================================
// 函数生成
// ============================================================================

/**
 * @brief 生成单个函数的汇编代码
 * 
 * 处理流程：
 * 1. 初始化函数上下文（清空位置映射，设置栈偏移）
 * 2. 生成函数序言（保存寄存器，分配栈帧）
 * 3. 处理参数（前6个通过寄存器，其余通过栈）
 * 4. 翻译函数体中的所有四元式
 * 5. 生成函数尾声（恢复寄存器，返回）
 */
void CodeGenerator::emitFunction(const semantic::FunctionIR& func) {
    // 1. 初始化函数上下文
    currentFunction_ = func.name;
    inFunction_ = true;
    locations_.clear();
    localOffset_ = -40;  // 跳过5个 callee-saved 寄存器（每个8字节）
    
    // 2. 生成函数序言
    emitPrologue(func);
    
    // 3. 处理参数：System V AMD64 ABI
    //    - 前 6 个整数/指针参数：RDI, RSI, RDX, RCX, R8, R9
    //    - 前 8 个浮点参数：XMM0-XMM7
    //    - 超过限制的参数通过栈传递
    static const Register intParamRegs[] = {
        Register::RDI, Register::RSI, Register::RDX,
        Register::RCX, Register::R8, Register::R9
    };
    static const XmmRegister floatParamRegs[] = {
        XmmRegister::XMM0, XmmRegister::XMM1, XmmRegister::XMM2, XmmRegister::XMM3,
        XmmRegister::XMM4, XmmRegister::XMM5, XmmRegister::XMM6, XmmRegister::XMM7
    };
    
    int intParamIdx = 0;    // 整数参数计数
    int floatParamIdx = 0;  // 浮点参数计数
    int stackParamOffset = 16;  // 栈参数从 RBP+16 开始
    
    for (size_t i = 0; i < func.parameters.size(); ++i) {
        const auto& [name, type] = func.parameters[i];
        int size = getTypeSize(type);
        bool isFloat = type && type->isFloat();
        
        if (isFloat && floatParamIdx < 8) {
            // 浮点参数：通过 XMM 寄存器传递
            int off = allocateStack(size);
            locations_[name] = Location(off);
            emitLine("movsd " + xmmName(floatParamRegs[floatParamIdx]) + ", " + std::to_string(off) + "(%rbp)");
            floatParamIdx++;
        } else if (!isFloat && intParamIdx < 6) {
            // 整数/指针参数：通过通用寄存器传递
            int off = allocateStack(size);
            locations_[name] = Location(off);
            emitLine("movq " + regName(intParamRegs[intParamIdx]) + ", " + std::to_string(off) + "(%rbp)");
            intParamIdx++;
        } else {
            // 栈参数：直接使用调用者分配的位置
            locations_[name] = Location(stackParamOffset);
            stackParamOffset += 8;
        }
    }
    
    // 4. 翻译函数体
    for (const auto& quad : func.code) {
        translateQuad(quad);
    }
    
    // 5. 生成函数出口和尾声
    emitLabel("." + currentFunction_ + "_exit");
    emitEpilogue();
    
    emitDirective(".size " + currentFunction_ + ", .-" + currentFunction_);
    inFunction_ = false;
}

void CodeGenerator::emitPrologue(const semantic::FunctionIR& func) {
    emitDirective(".globl " + func.name);
    emitDirective(".type " + func.name + ", @function");
    emitLabel(func.name);
    
    // 标准函数序言：保存旧帧指针，设置新帧指针
    emitLine("pushq %rbp");
    emitLine("movq %rsp, %rbp");
    
    // 保存 callee-saved 寄存器（在栈帧分配之前）
    emitLine("pushq %rbx");
    emitLine("pushq %r12");
    emitLine("pushq %r13");
    emitLine("pushq %r14");
    emitLine("pushq %r15");
    
    // 计算栈帧大小：使用固定的较大值以容纳所有局部变量和临时变量
    // 代码生成器在翻译过程中动态分配栈空间，无法预知确切大小
    // 使用 1024 字节作为默认值，足以处理较复杂的函数
    // TODO: 实现两阶段生成，先用占位符，最后替换为实际大小
    int stackReserve = 1024;
    stackReserve = (stackReserve + 15) & ~15;  // 16 字节对齐
    stackSize_ = stackReserve;
    
    emitLine("subq $" + std::to_string(stackReserve) + ", %rsp");
}

void CodeGenerator::emitEpilogue() {
    // 恢复栈指针到 callee-saved 寄存器位置
    emitLine("leaq -40(%rbp), %rsp");
    
    // 恢复 callee-saved 寄存器（与保存顺序相反）
    emitLine("popq %r15");
    emitLine("popq %r14");
    emitLine("popq %r13");
    emitLine("popq %r12");
    emitLine("popq %rbx");
    
    // 恢复帧指针并返回
    emitLine("popq %rbp");
    emitLine("ret");
}

// ============================================================================
// 操作数处理
// ============================================================================

Location CodeGenerator::getLocation(const semantic::Operand& operand) {
    using Kind = semantic::OperandKind;
    
    switch (operand.kind) {
        case Kind::IntConst:
            return Location(std::to_string(operand.intValue), true);
        
        case Kind::FloatConst:
            return Location(std::to_string(operand.floatValue), true);
        
        case Kind::StringConst:
            return Location(operand.name, false);
        
        case Kind::Label:
            return Location(operand.name, false);
        
        case Kind::Global:
            return Location(operand.name, false);
        
        case Kind::Temp:
        case Kind::Variable: {
            auto it = locations_.find(operand.name);
            if (it != locations_.end()) {
                return it->second;
            }
            int size = getTypeSize(operand.type);
            // 如果类型大小为 0（如函数类型），使用默认指针大小
            if (size <= 0) size = 8;
            int off = allocateStack(size);
            locations_[operand.name] = Location(off);
            return Location(off);
        }
        
        default:
            return Location();
    }
}

std::string CodeGenerator::operandToAsm(const semantic::Operand& operand, int size) {
    (void)size;
    using Kind = semantic::OperandKind;
    
    switch (operand.kind) {
        case Kind::IntConst:
            return "$" + std::to_string(operand.intValue);
        case Kind::FloatConst: {
            // 浮点数常量需要通过标签引用
            std::string label = const_cast<CodeGenerator*>(this)->getFloatLabel(operand.floatValue);
            return label + "(%rip)";
        }
        default:
            return getLocation(operand).toAsm();
    }
}

Register CodeGenerator::loadToRegister(const semantic::Operand& operand) {
    Register reg = regAlloc_.allocate();
    if (reg == Register::NONE) {
        reg = Register::RAX;
    }
    
    using Kind = semantic::OperandKind;
    
    switch (operand.kind) {
        case Kind::IntConst:
            emitLine("movq $" + std::to_string(operand.intValue) + ", " + regName(reg));
            break;
        
        case Kind::FloatConst: {
            // 浮点数常量需要从数据段加载
            std::string label = getFloatLabel(operand.floatValue);
            emitLine("movq " + label + "(%rip), " + regName(reg));
            break;
        }
        
        case Kind::StringConst:
            emitLine("leaq " + operand.name + "(%rip), " + regName(reg));
            break;
        
        case Kind::Global:
            // 数组类型需要加载地址（数组衰减为指针）
            if (operand.type && operand.type->isArray()) {
                emitLine("leaq " + operand.name + "(%rip), " + regName(reg));
            }
            // 字符串字面量标签（.LC开头）需要加载地址
            else if (operand.name.size() >= 3 && operand.name[0] == '.' && operand.name[1] == 'L' && operand.name[2] == 'C') {
                emitLine("leaq " + operand.name + "(%rip), " + regName(reg));
            } else {
                emitLine("movq " + operand.name + "(%rip), " + regName(reg));
            }
            break;

        case Kind::Label:
            // 函数标签需要加载其地址
            emitLine("leaq " + operand.name + "(%rip), " + regName(reg));
            break;

        default: {
            Location loc = getLocation(operand);
            // 数组类型需要加载地址而不是内容（数组衰减为指针）
            if (operand.type && operand.type->isArray()) {
                emitLine("leaq " + loc.toAsm() + ", " + regName(reg));
            } else {
                emitLine("movq " + loc.toAsm() + ", " + regName(reg));
            }
            break;
        }
    }
    
    return reg;
}

void CodeGenerator::storeFromRegister(Register reg, const semantic::Operand& dest) {
    using Kind = semantic::OperandKind;
    
    if (dest.kind == Kind::Global) {
        emitLine("movq " + regName(reg) + ", " + dest.name + "(%rip)");
    } else {
        Location loc = getLocation(dest);
        emitLine("movq " + regName(reg) + ", " + loc.toAsm());
    }
}

// ============================================================================
// 四元式翻译
// ============================================================================

void CodeGenerator::translateQuad(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    switch (quad.opcode) {
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Mod:
            translateArithmetic(quad);
            break;
        case Op::Neg:
            translateNeg(quad);
            break;
            
        // 浮点运算
        case Op::FAdd:
        case Op::FSub:
        case Op::FMul:
        case Op::FDiv:
            translateFloatArithmetic(quad);
            break;
        case Op::FNeg:
            translateFloatNeg(quad);
            break;
            
        case Op::BitAnd:
        case Op::BitOr:
        case Op::BitXor:
        case Op::Shl:
        case Op::Shr:
            translateBitwise(quad);
            break;
        case Op::BitNot:
            translateBitNot(quad);
            break;
            
        case Op::Lt:
        case Op::Gt:
        case Op::Le:
        case Op::Ge:
        case Op::Eq:
        case Op::Ne:
            translateComparison(quad);
            break;
            
        // 浮点比较
        case Op::FLt:
        case Op::FGt:
        case Op::FLe:
        case Op::FGe:
        case Op::FEq:
        case Op::FNe:
            translateFloatComparison(quad);
            break;
            
        case Op::LogicalAnd:
        case Op::LogicalOr:
            translateLogical(quad);
            break;
        case Op::LogicalNot:
            translateLogNot(quad);
            break;
            
        case Op::Assign:
            translateAssign(quad);
            break;
        case Op::Load:
            translateLoad(quad);
            break;
        case Op::Store:
            translateStore(quad);
            break;
        case Op::LoadAddr:
            translateLoadAddr(quad);
            break;
        case Op::IndexAddr:
            translateIndexAddr(quad);
            break;
        case Op::MemberAddr:
            translateMemberAddr(quad);
            break;
            
        case Op::IntToFloat:
        case Op::FloatToInt:
        case Op::IntExtend:
        case Op::IntTrunc:
        case Op::PtrToInt:
        case Op::IntToPtr:
            translateCast(quad);
            break;
            
        case Op::Jump:
            translateJump(quad);
            break;
        case Op::JumpTrue:
            translateJumpTrue(quad);
            break;
        case Op::JumpFalse:
            translateJumpFalse(quad);
            break;
        case Op::Label:
            translateLabel(quad);
            break;
            
        case Op::Param:
            translateParam(quad);
            break;
        case Op::Call:
            translateCall(quad);
            break;
        case Op::Return:
            translateReturn(quad);
            break;
            
        case Op::Nop:
            emitLine("nop");
            break;
            
        case Op::Comment:
            emitComment(quad.arg1.name);
            break;
            
        default:
            emitComment("Unknown opcode");
            break;
    }
}

// ============================================================================
// 算术运算
// ============================================================================

void CodeGenerator::translateArithmetic(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    Register left = loadToRegister(quad.arg1);
    Register right = loadToRegister(quad.arg2);
    
    switch (quad.opcode) {
        case Op::Add:
            emitLine("addq " + regName(right) + ", " + regName(left));
            break;
        case Op::Sub:
            emitLine("subq " + regName(right) + ", " + regName(left));
            break;
        case Op::Mul:
            emitLine("imulq " + regName(right) + ", " + regName(left));
            break;
        case Op::Div:
        case Op::Mod: {
            emitLine("movq " + regName(left) + ", %rax");
            emitLine("cqto");
            emitLine("idivq " + regName(right));
            if (quad.opcode == Op::Div) {
                emitLine("movq %rax, " + regName(left));
            } else {
                emitLine("movq %rdx, " + regName(left));
            }
            break;
        }
        default:
            break;
    }
    
    storeFromRegister(left, quad.result);
    regAlloc_.release(left);
    regAlloc_.release(right);
}

void CodeGenerator::translateNeg(const semantic::Quadruple& quad) {
    Register reg = loadToRegister(quad.arg1);
    emitLine("negq " + regName(reg));
    storeFromRegister(reg, quad.result);
    regAlloc_.release(reg);
}

// ============================================================================
// 位运算
// ============================================================================

void CodeGenerator::translateBitwise(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    Register left = loadToRegister(quad.arg1);
    
    if (quad.opcode == Op::Shl || quad.opcode == Op::Shr) {
        emitLine("movq " + operandToAsm(quad.arg2) + ", %rcx");
        if (quad.opcode == Op::Shl) {
            emitLine("salq %cl, " + regName(left));
        } else {
            emitLine("sarq %cl, " + regName(left));
        }
    } else {
        Register right = loadToRegister(quad.arg2);
        switch (quad.opcode) {
            case Op::BitAnd:
                emitLine("andq " + regName(right) + ", " + regName(left));
                break;
            case Op::BitOr:
                emitLine("orq " + regName(right) + ", " + regName(left));
                break;
            case Op::BitXor:
                emitLine("xorq " + regName(right) + ", " + regName(left));
                break;
            default:
                break;
        }
        regAlloc_.release(right);
    }
    
    storeFromRegister(left, quad.result);
    regAlloc_.release(left);
}

void CodeGenerator::translateBitNot(const semantic::Quadruple& quad) {
    Register reg = loadToRegister(quad.arg1);
    emitLine("notq " + regName(reg));
    storeFromRegister(reg, quad.result);
    regAlloc_.release(reg);
}

// ============================================================================
// 比较运算
// ============================================================================

void CodeGenerator::translateComparison(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    Register left = loadToRegister(quad.arg1);
    Register right = loadToRegister(quad.arg2);
    
    emitLine("cmpq " + regName(right) + ", " + regName(left));
    
    const char* setcc = nullptr;
    switch (quad.opcode) {
        case Op::Lt: setcc = "setl"; break;
        case Op::Gt: setcc = "setg"; break;
        case Op::Le: setcc = "setle"; break;
        case Op::Ge: setcc = "setge"; break;
        case Op::Eq: setcc = "sete"; break;
        case Op::Ne: setcc = "setne"; break;
        default: break;
    }
    
    if (setcc) {
        // 先执行 setcc（使用 cmpq 设置的标志位）
        // 然后用 movzbl 零扩展到 32/64 位（movzbl 会自动清零高32位）
        emitLine(std::string(setcc) + " " + regName(left, 1));
        emitLine("movzbl " + regName(left, 1) + ", " + regName(left, 4));
    }
    
    storeFromRegister(left, quad.result);
    regAlloc_.release(left);
    regAlloc_.release(right);
}

// ============================================================================
// 逻辑运算
// ============================================================================

void CodeGenerator::translateLogical(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    Register left = loadToRegister(quad.arg1);
    Register right = loadToRegister(quad.arg2);
    
    emitLine("testq " + regName(left) + ", " + regName(left));
    emitLine("setne " + regName(left, 1));
    emitLine("testq " + regName(right) + ", " + regName(right));
    emitLine("setne " + regName(right, 1));
    
    if (quad.opcode == Op::LogicalAnd) {
        emitLine("andb " + regName(right, 1) + ", " + regName(left, 1));
    } else {
        emitLine("orb " + regName(right, 1) + ", " + regName(left, 1));
    }
    
    emitLine("movzbq " + regName(left, 1) + ", " + regName(left));
    
    storeFromRegister(left, quad.result);
    regAlloc_.release(left);
    regAlloc_.release(right);
}

void CodeGenerator::translateLogNot(const semantic::Quadruple& quad) {
    Register reg = loadToRegister(quad.arg1);
    emitLine("testq " + regName(reg) + ", " + regName(reg));
    emitLine("sete " + regName(reg, 1));
    emitLine("movzbq " + regName(reg, 1) + ", " + regName(reg));
    storeFromRegister(reg, quad.result);
    regAlloc_.release(reg);
}

// ============================================================================
// 赋值和内存
// ============================================================================

void CodeGenerator::translateAssign(const semantic::Quadruple& quad) {
    // 检查类型大小，处理结构体赋值
    int typeSize = quad.arg1.type ? quad.arg1.type->size() : 8;
    
    if (typeSize > 8) {
        // 大于 8 字节的结构体：逐 8 字节复制
        Location srcLoc = getLocation(quad.arg1);
        Location dstLoc = getLocation(quad.result);
        
        for (int offset = 0; offset < typeSize; offset += 8) {
            int copySize = std::min(8, typeSize - offset);
            std::string srcAsm = std::to_string(srcLoc.offset + offset) + "(%rbp)";
            std::string dstAsm = std::to_string(dstLoc.offset + offset) + "(%rbp)";
            
            if (copySize >= 8) {
                emitLine("movq " + srcAsm + ", %r10");
                emitLine("movq %r10, " + dstAsm);
            } else if (copySize >= 4) {
                emitLine("movl " + srcAsm + ", %r10d");
                emitLine("movl %r10d, " + dstAsm);
            } else {
                // 处理剩余的小块
                for (int i = 0; i < copySize; ++i) {
                    std::string srcByte = std::to_string(srcLoc.offset + offset + i) + "(%rbp)";
                    std::string dstByte = std::to_string(dstLoc.offset + offset + i) + "(%rbp)";
                    emitLine("movb " + srcByte + ", %r10b");
                    emitLine("movb %r10b, " + dstByte);
                }
            }
        }
    } else {
        Register reg = loadToRegister(quad.arg1);
        storeFromRegister(reg, quad.result);
        regAlloc_.release(reg);
    }
}

void CodeGenerator::translateLoad(const semantic::Quadruple& quad) {
    // 根据结果类型选择正确的加载大小
    int size = 8;  // 默认 64 位
    if (quad.result.type) {
        size = getTypeSize(quad.result.type);
    }
    
    if (size > 8) {
        // 大于 8 字节的结构体：从源地址逐块复制到目标
        Register addr = loadToRegister(quad.arg1);
        Location dstLoc = getLocation(quad.result);
        
        // 保存地址到 %r11，因为 %r10 会被用于复制数据
        emitLine("movq " + regName(addr) + ", %r11");
        regAlloc_.release(addr);
        
        for (int offset = 0; offset < size; offset += 8) {
            int copySize = std::min(8, size - offset);
            std::string srcAsm = std::to_string(offset) + "(%r11)";
            std::string dstAsm = std::to_string(dstLoc.offset + offset) + "(%rbp)";
            
            if (copySize >= 8) {
                emitLine("movq " + srcAsm + ", %r10");
                emitLine("movq %r10, " + dstAsm);
            } else if (copySize >= 4) {
                emitLine("movl " + srcAsm + ", %r10d");
                emitLine("movl %r10d, " + dstAsm);
            }
        }
    } else {
        Register addr = loadToRegister(quad.arg1);

        switch (size) {
            case 1:
                emitLine("movzbl (" + regName(addr) + "), " + regName(addr, 4));
                break;
            case 2:
                emitLine("movzwl (" + regName(addr) + "), " + regName(addr, 4));
                break;
            case 4:
                emitLine("movl (" + regName(addr) + "), " + regName(addr, 4));
                break;
            default:
                emitLine("movq (" + regName(addr) + "), " + regName(addr));
                break;
        }
        storeFromRegister(addr, quad.result);
        regAlloc_.release(addr);
    }
}

void CodeGenerator::translateStore(const semantic::Quadruple& quad) {
    Register val = loadToRegister(quad.arg1);
    Register addr = loadToRegister(quad.result);
    
    // 根据地址指向的类型选择正确的存储大小
    int size = 8;  // 默认 64 位
    if (quad.result.type && quad.result.type->isPointer()) {
        auto ptrType = static_cast<semantic::PointerType*>(quad.result.type.get());
        if (ptrType->pointee) {
            size = ptrType->pointee->size();
        }
    } else if (quad.arg1.type) {
        size = getTypeSize(quad.arg1.type);
    }
    
    std::string suffix = getSizeSuffix(size);
    emitLine("mov" + suffix + " " + regName(val, size) + ", (" + regName(addr) + ")");
    regAlloc_.release(val);
    regAlloc_.release(addr);
}

void CodeGenerator::translateLoadAddr(const semantic::Quadruple& quad) {
    Register reg = regAlloc_.allocate();
    if (reg == Register::NONE) reg = Register::RAX;

    using Kind = semantic::OperandKind;
    if (quad.arg1.kind == Kind::Global) {
        emitLine("leaq " + quad.arg1.name + "(%rip), " + regName(reg));
    } else {
        Location loc = getLocation(quad.arg1);
        emitLine("leaq " + loc.toAsm() + ", " + regName(reg));
    }

    storeFromRegister(reg, quad.result);
    regAlloc_.release(reg);
}

void CodeGenerator::translateIndexAddr(const semantic::Quadruple& quad) {
    Register base = loadToRegister(quad.arg1);
    Register index = loadToRegister(quad.arg2);
    
    // quad.result.type 是指针类型，需要获取其指向的元素大小
    int elemSize = 8;  // 默认指针大小
    if (quad.result.type && quad.result.type->isPointer()) {
        auto ptrType = static_cast<semantic::PointerType*>(quad.result.type.get());
        if (ptrType->pointee) {
            elemSize = ptrType->pointee->size();
        }
    }
    
    if (elemSize != 1) {
        emitLine("imulq $" + std::to_string(elemSize) + ", " + regName(index));
    }
    emitLine("addq " + regName(index) + ", " + regName(base));
    
    storeFromRegister(base, quad.result);
    regAlloc_.release(base);
    regAlloc_.release(index);
}

void CodeGenerator::translateMemberAddr(const semantic::Quadruple& quad) {
    Register base = loadToRegister(quad.arg1);
    
    if (quad.arg2.kind == semantic::OperandKind::IntConst && quad.arg2.intValue != 0) {
        emitLine("addq $" + std::to_string(quad.arg2.intValue) + ", " + regName(base));
    }
    
    storeFromRegister(base, quad.result);
    regAlloc_.release(base);
}

/**
 * @brief 翻译类型转换
 * 
 * 支持的转换：
 * - IntToFloat: 整数转浮点（cvtsi2sdq）
 * - FloatToInt: 浮点转整数（cvttsd2siq，向零截断）
 * - IntExtend: 整数符号扩展（movsx/movsxd）
 * - IntTrunc: 整数截断（直接使用低位）
 * - PtrToInt/IntToPtr: 指针与整数互转（位模式不变）
 */
void CodeGenerator::translateCast(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    if (quad.opcode == Op::IntToFloat) {
        // 整数转浮点：加载到通用寄存器，然后转换到 XMM
        Register intReg = loadToRegister(quad.arg1);
        XmmRegister xmmReg = xmmAlloc_.allocate();
        if (xmmReg == XmmRegister::NONE) xmmReg = XmmRegister::XMM0;
        
        // cvtsi2sdq: 64 位有符号整数转双精度浮点
        emitLine("cvtsi2sdq " + regName(intReg) + ", " + xmmName(xmmReg));
        storeFromXmm(xmmReg, quad.result);
        
        regAlloc_.release(intReg);
        xmmAlloc_.release(xmmReg);
    } else if (quad.opcode == Op::FloatToInt) {
        // 浮点转整数：加载到 XMM，然后转换到通用寄存器
        XmmRegister xmmReg = loadToXmm(quad.arg1);
        Register intReg = regAlloc_.allocate();
        if (intReg == Register::NONE) intReg = Register::RAX;
        
        // cvttsd2siq: 双精度浮点转 64 位有符号整数（截断，向零舍入）
        emitLine("cvttsd2siq " + xmmName(xmmReg) + ", " + regName(intReg));
        storeFromRegister(intReg, quad.result);
        
        xmmAlloc_.release(xmmReg);
        regAlloc_.release(intReg);
    } else if (quad.opcode == Op::IntExtend) {
        // 整数符号扩展
        Register reg = loadToRegister(quad.arg1);
        int srcSize = quad.arg1.type ? quad.arg1.type->size() : 4;
        
        // 根据源大小选择扩展指令
        if (srcSize == 1) {
            emitLine("movsbq " + regName(reg, 1) + ", " + regName(reg));
        } else if (srcSize == 2) {
            emitLine("movswq " + regName(reg, 2) + ", " + regName(reg));
        } else if (srcSize == 4) {
            emitLine("movslq " + regName(reg, 4) + ", " + regName(reg));
        }
        // srcSize == 8 不需要扩展
        
        storeFromRegister(reg, quad.result);
        regAlloc_.release(reg);
    } else if (quad.opcode == Op::IntTrunc) {
        // 整数截断：直接加载和存储（高位自动丢弃）
        Register reg = loadToRegister(quad.arg1);
        storeFromRegister(reg, quad.result);
        regAlloc_.release(reg);
    } else if (quad.opcode == Op::PtrToInt || quad.opcode == Op::IntToPtr) {
        // 指针与整数互转：位模式不变，直接赋值
        Register reg = loadToRegister(quad.arg1);
        storeFromRegister(reg, quad.result);
        regAlloc_.release(reg);
    } else {
        // 其他转换作为简单赋值处理
        translateAssign(quad);
    }
}

// ============================================================================
// 控制流
// ============================================================================

void CodeGenerator::translateJump(const semantic::Quadruple& quad) {
    emitLine("jmp ." + currentFunction_ + "_lbl_" + quad.result.name);
}

void CodeGenerator::translateJumpTrue(const semantic::Quadruple& quad) {
    Register reg = loadToRegister(quad.arg1);
    emitLine("testq " + regName(reg) + ", " + regName(reg));
    emitLine("jnz ." + currentFunction_ + "_lbl_" + quad.result.name);
    regAlloc_.release(reg);
}

void CodeGenerator::translateJumpFalse(const semantic::Quadruple& quad) {
    Register reg = loadToRegister(quad.arg1);
    emitLine("testq " + regName(reg) + ", " + regName(reg));
    emitLine("jz ." + currentFunction_ + "_lbl_" + quad.result.name);
    regAlloc_.release(reg);
}

void CodeGenerator::translateLabel(const semantic::Quadruple& quad) {
    emitLabel("." + currentFunction_ + "_lbl_" + quad.result.name);
}

// ============================================================================
// 函数调用
// ============================================================================

void CodeGenerator::translateParam(const semantic::Quadruple& quad) {
    callParams_.push_back(quad.arg1);
}

/**
 * @brief 翻译函数调用
 * 
 * System V AMD64 ABI 参数传递规则：
 * - 整数/指针参数：前 6 个使用 RDI, RSI, RDX, RCX, R8, R9
 * - 浮点参数：前 8 个使用 XMM0-XMM7
 * - 超过限制的参数通过栈传递（从右到左入栈）
 * - AL 寄存器存放使用的 XMM 寄存器数量（变参函数）
 * 
 * 处理流程：
 * 1. 反转参数顺序（IR 生成时是逆序的）
 * 2. 分类参数（整数/浮点/栈）
 * 3. 栈对齐：确保 16 字节对齐
 * 4. 栈参数入栈
 * 5. 寄存器参数设置
 * 6. 执行调用
 * 7. 清理栈参数
 * 8. 处理返回值
 */
void CodeGenerator::translateCall(const semantic::Quadruple& quad) {
    static const Register intParamRegs[] = {
        Register::RDI, Register::RSI, Register::RDX,
        Register::RCX, Register::R8, Register::R9
    };
    static const XmmRegister floatParamRegs[] = {
        XmmRegister::XMM0, XmmRegister::XMM1, XmmRegister::XMM2, XmmRegister::XMM3,
        XmmRegister::XMM4, XmmRegister::XMM5, XmmRegister::XMM6, XmmRegister::XMM7
    };
    
    int numParams = static_cast<int>(callParams_.size());
    
    // 1. 反转参数顺序（IR 生成时参数是逆序 PARAM 的）
    std::reverse(callParams_.begin(), callParams_.end());
    
    // 2. 分类参数：统计整数/浮点参数数量，确定哪些参数需要入栈
    int intParamCount = 0;
    int floatParamCount = 0;
    std::vector<int> stackParamIndices;
    
    for (int i = 0; i < numParams; ++i) {
        bool isFloat = callParams_[i].type && callParams_[i].type->isFloat();
        if (isFloat) {
            if (floatParamCount < 8) {
                floatParamCount++;
            } else {
                stackParamIndices.push_back(i);
            }
        } else {
            if (intParamCount < 6) {
                intParamCount++;
            } else {
                stackParamIndices.push_back(i);
            }
        }
    }
    
    // 3. 栈对齐：System V ABI 要求调用前栈 16 字节对齐
    int stackParamNum = static_cast<int>(stackParamIndices.size());
    if (stackParamNum % 2 != 0) {
        emitLine("subq $8, %rsp");
    }
    
    // 4. 栈参数：从右到左入栈
    for (int i = stackParamNum - 1; i >= 0; --i) {
        int idx = stackParamIndices[i];
        bool isFloat = callParams_[idx].type && callParams_[idx].type->isFloat();
        if (isFloat) {
            XmmRegister xmm = loadToXmm(callParams_[idx]);
            emitLine("subq $8, %rsp");
            emitLine("movsd " + xmmName(xmm) + ", (%rsp)");
            xmmAlloc_.release(xmm);
        } else {
            Register reg = loadToRegister(callParams_[idx]);
            emitLine("pushq " + regName(reg));
            regAlloc_.release(reg);
        }
    }
    
    // 5. 寄存器参数设置
    int intIdx = 0;
    int floatIdx = 0;
    
    for (int i = 0; i < numParams; ++i) {
        using Kind = semantic::OperandKind;
        bool isFloat = callParams_[i].type && callParams_[i].type->isFloat();
        
        if (isFloat && floatIdx < 8) {
            // 浮点参数：放入 XMM 寄存器
            XmmRegister dest = floatParamRegs[floatIdx];
            if (callParams_[i].kind == Kind::FloatConst) {
                std::string label = getFloatLabel(callParams_[i].floatValue);
                emitLine("movsd " + label + "(%rip), " + xmmName(dest));
            } else {
                Location loc = getLocation(callParams_[i]);
                emitLine("movsd " + loc.toAsm() + ", " + xmmName(dest));
            }
            floatIdx++;
        } else if (!isFloat && intIdx < 6) {
            // 整数/指针参数：放入通用寄存器
            Register dest = intParamRegs[intIdx];
            
            if (callParams_[i].kind == Kind::IntConst) {
                emitLine("movq $" + std::to_string(callParams_[i].intValue) + ", " + regName(dest));
            } else if (callParams_[i].kind == Kind::FloatConst) {
                // 浮点常量但类型不是 float（不应该发生）
                std::string label = getFloatLabel(callParams_[i].floatValue);
                emitLine("movq " + label + "(%rip), " + regName(dest));
            } else if (callParams_[i].kind == Kind::StringConst) {
                emitLine("leaq " + callParams_[i].name + "(%rip), " + regName(dest));
            } else if (callParams_[i].kind == Kind::Label) {
                emitLine("leaq " + callParams_[i].name + "(%rip), " + regName(dest));
            } else if (callParams_[i].kind == Kind::Global) {
                if (callParams_[i].name.size() >= 3 && 
                    callParams_[i].name[0] == '.' && 
                    callParams_[i].name[1] == 'L' && 
                    callParams_[i].name[2] == 'C') {
                    emitLine("leaq " + callParams_[i].name + "(%rip), " + regName(dest));
                } else {
                    emitLine("movq " + callParams_[i].name + "(%rip), " + regName(dest));
                }
            } else {
                Location loc = getLocation(callParams_[i]);
                if (callParams_[i].type && callParams_[i].type->isArray()) {
                    emitLine("leaq " + loc.toAsm() + ", " + regName(dest));
                } else {
                    emitLine("movq " + loc.toAsm() + ", " + regName(dest));
                }
            }
            intIdx++;
        }
        // 栈参数已在步骤 4 处理
    }
    
    // 设置 AL 为使用的 XMM 寄存器数量（变参函数需要）
    emitLine("movl $" + std::to_string(floatIdx) + ", %eax");
    
    // 6. 执行调用
    using Kind = semantic::OperandKind;
    if (quad.arg1.kind == Kind::Label) {
        emitLine("call " + quad.arg1.name);
    } else {
        Register funcReg = loadToRegister(quad.arg1);
        emitLine("call *" + regName(funcReg));
        regAlloc_.release(funcReg);
    }
    
    // 7. 清理栈参数
    if (stackParamNum > 0) {
        int cleanup = stackParamNum * 8;
        if (stackParamNum % 2 != 0) cleanup += 8;
        emitLine("addq $" + std::to_string(cleanup) + ", %rsp");
    }
    
    // 8. 处理返回值
    if (!quad.result.isNone()) {
        bool isFloatReturn = quad.result.type && quad.result.type->isFloat();
        
        if (isFloatReturn) {
            // 浮点返回值在 XMM0
            Location loc = getLocation(quad.result);
            emitLine("movsd %xmm0, " + loc.toAsm());
        } else {
            int typeSize = quad.result.type ? quad.result.type->size() : 8;
            
            if (typeSize > 8 && typeSize <= 16) {
                // 16 字节以内结构体：RAX + RDX
                Location loc = getLocation(quad.result);
                emitLine("movq %rax, " + loc.toAsm());
                emitLine("movq %rdx, " + std::to_string(loc.offset + 8) + "(%rbp)");
            } else if (typeSize == 4) {
                emitLine("cltq");
                storeFromRegister(Register::RAX, quad.result);
            } else {
                storeFromRegister(Register::RAX, quad.result);
            }
        }
    }
    
    callParams_.clear();
}

void CodeGenerator::translateReturn(const semantic::Quadruple& quad) {
    if (!quad.arg1.isNone()) {
        bool isFloat = quad.arg1.type && quad.arg1.type->isFloat();
        
        if (isFloat) {
            // 浮点返回值：放入 XMM0
            XmmRegister xmm = loadToXmm(quad.arg1);
            if (xmm != XmmRegister::XMM0) {
                emitLine("movsd " + xmmName(xmm) + ", %xmm0");
            }
            xmmAlloc_.release(xmm);
        } else {
            int typeSize = quad.arg1.type ? quad.arg1.type->size() : 8;
            
            if (typeSize > 8 && typeSize <= 16) {
                // 16 字节以内的结构体：低 8 字节在 RAX，高 8 字节在 RDX
                Location loc = getLocation(quad.arg1);
                emitLine("movq " + loc.toAsm() + ", %rax");
                emitLine("movq " + std::to_string(loc.offset + 8) + "(%rbp), %rdx");
            } else {
                Register reg = loadToRegister(quad.arg1);
                if (reg != Register::RAX) {
                    emitLine("movq " + regName(reg) + ", %rax");
                }
                regAlloc_.release(reg);
            }
        }
    }
    
    emitLine("jmp ." + currentFunction_ + "_exit");
}

// ============================================================================
// 辅助方法
// ============================================================================

int CodeGenerator::allocateStack(int size, int align) {
    localOffset_ -= size;
    // 向下对齐（对于负数需要特殊处理）
    // -44 应该对齐到 -48，而不是 -40
    if (localOffset_ < 0) {
        localOffset_ = -(((-localOffset_) + align - 1) / align * align);
    } else {
        localOffset_ = (localOffset_ / align) * align;
    }
    return localOffset_;
}

void CodeGenerator::alignStack(int align) {
    // 向下对齐（对于负数需要特殊处理）
    if (localOffset_ < 0) {
        localOffset_ = -(((-localOffset_) + align - 1) / align * align);
    } else {
        localOffset_ = (localOffset_ / align) * align;
    }
}

int CodeGenerator::getTypeSize(semantic::TypePtr type) const {
    if (!type) return 8;
    return type->size();
}

std::string CodeGenerator::getSizeSuffix(int size) const {
    switch (size) {
        case 1: return "b";
        case 2: return "w";
        case 4: return "l";
        default: return "q";
    }
}

// ============================================================================
// 浮点运算
// ============================================================================

bool CodeGenerator::isFloatType(const semantic::Operand& operand) const {
    if (operand.kind == semantic::OperandKind::FloatConst) {
        return true;
    }
    if (operand.type) {
        return operand.type->isFloat();
    }
    return false;
}

XmmRegister CodeGenerator::loadToXmm(const semantic::Operand& operand) {
    XmmRegister reg = xmmAlloc_.allocate();
    if (reg == XmmRegister::NONE) {
        reg = XmmRegister::XMM0;
    }
    
    using Kind = semantic::OperandKind;
    
    if (operand.kind == Kind::FloatConst) {
        // 浮点常量：从 .rodata 段加载
        std::string label = const_cast<CodeGenerator*>(this)->getFloatLabel(operand.floatValue);
        emitLine("movsd " + label + "(%rip), " + xmmName(reg));
    } else if (operand.kind == Kind::Global) {
        emitLine("movsd " + operand.name + "(%rip), " + xmmName(reg));
    } else {
        Location loc = getLocation(operand);
        emitLine("movsd " + loc.toAsm() + ", " + xmmName(reg));
    }
    
    return reg;
}

void CodeGenerator::storeFromXmm(XmmRegister reg, const semantic::Operand& dest) {
    using Kind = semantic::OperandKind;
    
    if (dest.kind == Kind::Global) {
        emitLine("movsd " + xmmName(reg) + ", " + dest.name + "(%rip)");
    } else {
        Location loc = getLocation(dest);
        emitLine("movsd " + xmmName(reg) + ", " + loc.toAsm());
    }
}

/**
 * @brief 翻译浮点算术运算
 * 
 * 使用 XMM 寄存器和 SSE 指令：
 * - addsd: 双精度加法
 * - subsd: 双精度减法
 * - mulsd: 双精度乘法
 * - divsd: 双精度除法
 */
void CodeGenerator::translateFloatArithmetic(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    XmmRegister left = loadToXmm(quad.arg1);
    XmmRegister right = loadToXmm(quad.arg2);
    
    switch (quad.opcode) {
        case Op::FAdd:
            emitLine("addsd " + xmmName(right) + ", " + xmmName(left));
            break;
        case Op::FSub:
            emitLine("subsd " + xmmName(right) + ", " + xmmName(left));
            break;
        case Op::FMul:
            emitLine("mulsd " + xmmName(right) + ", " + xmmName(left));
            break;
        case Op::FDiv:
            emitLine("divsd " + xmmName(right) + ", " + xmmName(left));
            break;
        default:
            break;
    }
    
    storeFromXmm(left, quad.result);
    xmmAlloc_.release(left);
    xmmAlloc_.release(right);
}

/**
 * @brief 翻译浮点取负运算
 * 
 * 使用异或操作翻转符号位：xorpd 与符号掩码
 */
void CodeGenerator::translateFloatNeg(const semantic::Quadruple& quad) {
    XmmRegister reg = loadToXmm(quad.arg1);
    
    // 获取或创建符号掩码（最高位为 1，其余为 0）
    // -0.0 的 IEEE 754 表示是 0x8000000000000000
    std::string maskLabel = getFloatLabel(-0.0);
    
    XmmRegister mask = xmmAlloc_.allocate();
    if (mask == XmmRegister::NONE) mask = XmmRegister::XMM1;
    
    emitLine("movsd " + maskLabel + "(%rip), " + xmmName(mask));
    emitLine("xorpd " + xmmName(mask) + ", " + xmmName(reg));
    
    storeFromXmm(reg, quad.result);
    xmmAlloc_.release(reg);
    xmmAlloc_.release(mask);
}

/**
 * @brief 翻译浮点比较运算
 * 
 * 使用 ucomisd 指令比较，然后根据条件设置结果：
 * - ucomisd: 无序比较（处理 NaN）
 * - seta/setae/setb/setbe: 无符号条件码（用于浮点比较）
 */
void CodeGenerator::translateFloatComparison(const semantic::Quadruple& quad) {
    using Op = semantic::IROpcode;
    
    XmmRegister left = loadToXmm(quad.arg1);
    XmmRegister right = loadToXmm(quad.arg2);
    
    Register result = regAlloc_.allocate();
    if (result == Register::NONE) result = Register::RAX;
    
    // 清零结果寄存器（必须在 ucomisd 之前，否则会破坏标志位）
    emitLine("xorl " + regName(result, 4) + ", " + regName(result, 4));
    
    // ucomisd 比较：左 - 右
    emitLine("ucomisd " + xmmName(right) + ", " + xmmName(left));
    
    // 根据比较类型设置结果
    const char* setcc = nullptr;
    switch (quad.opcode) {
        case Op::FLt:  setcc = "setb"; break;   // below (CF=1)
        case Op::FLe:  setcc = "setbe"; break;  // below or equal (CF=1 or ZF=1)
        case Op::FGt:  setcc = "seta"; break;   // above (CF=0 and ZF=0)
        case Op::FGe:  setcc = "setae"; break;  // above or equal (CF=0)
        case Op::FEq:  setcc = "sete"; break;   // equal (ZF=1)
        case Op::FNe:  setcc = "setne"; break;  // not equal (ZF=0)
        default: break;
    }
    
    if (setcc) {
        emitLine(std::string(setcc) + " " + regName(result, 1));
    }
    
    storeFromRegister(result, quad.result);
    
    xmmAlloc_.release(left);
    xmmAlloc_.release(right);
    regAlloc_.release(result);
}

// ============================================================================
// 汇编和链接
// ============================================================================

/**
 * @brief 汇编并链接生成可执行文件
 * 
 * 处理流程：
 * 1. 使用 GNU as 将汇编文件编译为目标文件
 * 2. 查找 libcdd.so 标准库位置
 * 3. 使用 gcc 链接目标文件和标准库
 */
bool assembleAndLink(const std::string& asmFile, const std::string& outputFile) {
    // 1. 汇编：.s -> .o
    std::string objFile = asmFile.substr(0, asmFile.rfind('.')) + ".o";
    std::string cmd = "as -o " + objFile + " " + asmFile;
    if (std::system(cmd.c_str()) != 0) {
        return false;
    }

    // 2. 查找 libcdd.so 位置（开发时可能在不同目录）
    std::string buildLibPath;
    if (std::ifstream("libcdd.so").good()) {
        buildLibPath = "-L. -Wl,-rpath,.";
    } else if (std::ifstream("../build/libcdd.so").good()) {
        buildLibPath = "-L../build -Wl,-rpath,../build";
    } else if (std::ifstream("build/libcdd.so").good()) {
        buildLibPath = "-Lbuild -Wl,-rpath,build";
    }
    
    // 3. 链接：.o + libcdd.so -> 可执行文件
    if (!buildLibPath.empty()) {
        cmd = "gcc -o " + outputFile + " " + objFile + " -no-pie " + buildLibPath + " -lcdd";
    } else {
        // 使用系统库路径（安装后）
        cmd = "gcc -o " + outputFile + " " + objFile + " -no-pie -lcdd";
    }
    
    if (std::system(cmd.c_str()) != 0) {
        return false;
    }

    return true;
}

} // namespace codegen
} // namespace cdd
