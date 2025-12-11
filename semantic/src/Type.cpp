/**
 * @file Type.cpp
 * @brief 类型系统实现
 * @author mozihe
 */

#include "Type.h"
#include <algorithm>

namespace cdd {
namespace semantic {

/**
 * @brief 检查两个类型是否兼容
 */
bool areTypesCompatible(const TypePtr& t1, const TypePtr& t2) {
    if (!t1 || !t2) return false;
    if (t1->kind != t2->kind) return false;
    
    switch (t1->kind) {
        case TypeKind::Void:
            return true;
            
        case TypeKind::Integer: {
            auto i1 = static_cast<IntegerType*>(t1.get());
            auto i2 = static_cast<IntegerType*>(t2.get());
            return i1->intKind == i2->intKind && i1->isUnsigned == i2->isUnsigned;
        }
        
        case TypeKind::Float: {
            auto f1 = static_cast<FloatType*>(t1.get());
            auto f2 = static_cast<FloatType*>(t2.get());
            return f1->floatKind == f2->floatKind;
        }
        
        case TypeKind::Pointer: {
            auto p1 = static_cast<PointerType*>(t1.get());
            auto p2 = static_cast<PointerType*>(t2.get());
            // void* 与任何指针兼容
            if (p1->pointee->isVoid() || p2->pointee->isVoid()) return true;
            return areTypesCompatible(p1->pointee, p2->pointee);
        }
        
        case TypeKind::Array: {
            auto a1 = static_cast<ArrayType*>(t1.get());
            auto a2 = static_cast<ArrayType*>(t2.get());
            if (!areTypesCompatible(a1->elementType, a2->elementType)) return false;
            // 不完整数组与任何长度数组兼容
            if (a1->length < 0 || a2->length < 0) return true;
            return a1->length == a2->length;
        }
        
        case TypeKind::Function: {
            auto f1 = static_cast<FunctionType*>(t1.get());
            auto f2 = static_cast<FunctionType*>(t2.get());
            if (!areTypesCompatible(f1->returnType, f2->returnType)) return false;
            if (f1->paramTypes.size() != f2->paramTypes.size()) return false;
            if (f1->isVariadic != f2->isVariadic) return false;
            for (size_t i = 0; i < f1->paramTypes.size(); ++i) {
                if (!areTypesCompatible(f1->paramTypes[i], f2->paramTypes[i])) {
                    return false;
                }
            }
            return true;
        }
        
        case TypeKind::Struct: {
            auto s1 = static_cast<StructType*>(t1.get());
            auto s2 = static_cast<StructType*>(t2.get());
            return s1->name == s2->name;  // 结构体按名称匹配
        }
        
        case TypeKind::Union: {
            auto u1 = static_cast<UnionType*>(t1.get());
            auto u2 = static_cast<UnionType*>(t2.get());
            return u1->name == u2->name;
        }
        
        case TypeKind::Enum: {
            auto e1 = static_cast<EnumType*>(t1.get());
            auto e2 = static_cast<EnumType*>(t2.get());
            return e1->name == e2->name;
        }
    }
    
    return false;
}

/**
 * @brief 检查能否从 from 类型隐式转换到 to 类型
 */
bool canImplicitlyConvert(const TypePtr& from, const TypePtr& to) {
    if (!from || !to) return false;
    if (areTypesCompatible(from, to)) return true;
    
    // void 不能隐式转换
    if (from->isVoid() || to->isVoid()) return false;
    
    // 整数之间可以隐式转换
    if (from->isInteger() && to->isInteger()) return true;
    
    // 浮点之间可以隐式转换
    if (from->isFloat() && to->isFloat()) return true;
    
    // 整数到浮点
    if (from->isInteger() && to->isFloat()) return true;
    
    // 浮点到整数（有警告，但允许）
    if (from->isFloat() && to->isInteger()) return true;
    
    // 枚举到整数
    if (from->isEnum() && to->isInteger()) return true;
    
    // 整数到枚举
    if (from->isInteger() && to->isEnum()) return true;
    
    // 指针与整数之间（有警告）
    if ((from->isPointer() && to->isInteger()) ||
        (from->isInteger() && to->isPointer())) {
        return true;
    }
    
    // 任何指针可以转换为 void*
    if (from->isPointer() && to->isPointer()) {
        auto toPtr = static_cast<PointerType*>(to.get());
        if (toPtr->pointee->isVoid()) return true;
    }
    
    // void* 可以转换为任何指针
    if (from->isPointer() && to->isPointer()) {
        auto fromPtr = static_cast<PointerType*>(from.get());
        if (fromPtr->pointee->isVoid()) return true;
    }
    
    // 数组到指针的隐式转换（数组衰减）
    if (from->isArray() && to->isPointer()) {
        auto arr = static_cast<ArrayType*>(from.get());
        auto ptr = static_cast<PointerType*>(to.get());
        return areTypesCompatible(arr->elementType, ptr->pointee);
    }
    
    // 字符串字面量（char*）到字符数组的转换
    // 这是 C 语言中 char s[] = "string" 的特殊情况
    if (from->isPointer() && to->isArray()) {
        auto ptr = static_cast<PointerType*>(from.get());
        auto arr = static_cast<ArrayType*>(to.get());
        // 只允许 char* 到 char[] 的转换
        if (ptr->pointee->isInteger() && arr->elementType->isInteger()) {
            auto ptrInt = static_cast<IntegerType*>(ptr->pointee.get());
            auto arrInt = static_cast<IntegerType*>(arr->elementType.get());
            if (ptrInt->intKind == IntegerKind::Char && arrInt->intKind == IntegerKind::Char) {
                return true;
            }
        }
    }
    
    // 函数到函数指针的隐式转换
    if (from->isFunction() && to->isPointer()) {
        auto ptr = static_cast<PointerType*>(to.get());
        if (ptr->pointee->isFunction()) {
            return areTypesCompatible(from, ptr->pointee);
        }
    }
    
    return false;
}

/**
 * @brief 获取两个类型的公共类型（用于二元运算）
 * 
 * 类型提升规则（按优先级）：
 * 1. 相同类型 -> 返回该类型
 * 2. 浮点类型 -> 返回更大的浮点类型
 * 3. 整数类型 -> 执行整数提升（至少 int）
 * 4. 枚举 -> 当作 int 处理
 * 5. 指针 + 整数 -> 返回指针类型
 */
TypePtr getCommonType(const TypePtr& t1, const TypePtr& t2) {
    if (!t1 || !t2) return nullptr;
    
    // 1. 相同类型
    if (areTypesCompatible(t1, t2)) {
        return t1->clone();
    }
    
    // 2. 浮点类型优先
    if (t1->isFloat() || t2->isFloat()) {
        if (t1->isFloat() && t2->isFloat()) {
            auto f1 = static_cast<FloatType*>(t1.get());
            auto f2 = static_cast<FloatType*>(t2.get());
            // 选择更大的浮点类型
            if (f1->floatKind >= f2->floatKind) {
                return t1->clone();
            } else {
                return t2->clone();
            }
        }
        // 整数与浮点，返回浮点类型
        return t1->isFloat() ? t1->clone() : t2->clone();
    }
    
    // 整数类型提升规则
    if (t1->isInteger() && t2->isInteger()) {
        auto i1 = static_cast<IntegerType*>(t1.get());
        auto i2 = static_cast<IntegerType*>(t2.get());
        
        // 确定结果类型
        IntegerKind resultKind = std::max(i1->intKind, i2->intKind);
        // 至少是 int
        if (resultKind < IntegerKind::Int) {
            resultKind = IntegerKind::Int;
        }
        
        // 确定符号性
        bool isUnsigned = false;
        if (i1->intKind == i2->intKind) {
            isUnsigned = i1->isUnsigned || i2->isUnsigned;
        } else if (i1->intKind > i2->intKind) {
            isUnsigned = i1->isUnsigned;
        } else {
            isUnsigned = i2->isUnsigned;
        }
        
        return std::make_shared<IntegerType>(resultKind, isUnsigned);
    }
    
    // 枚举当作 int 处理
    if (t1->isEnum() && t2->isInteger()) {
        return t2->clone();
    }
    if (t1->isInteger() && t2->isEnum()) {
        return t1->clone();
    }
    if (t1->isEnum() && t2->isEnum()) {
        return makeInt();
    }
    
    // 指针算术
    if ((t1->isPointer() && t2->isInteger()) ||
        (t1->isInteger() && t2->isPointer())) {
        return t1->isPointer() ? t1->clone() : t2->clone();
    }
    
    return nullptr;
}

} // namespace semantic
} // namespace cdd
