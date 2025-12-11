/**
 * @file Type.h
 * @brief 类型系统
 * @author mozihe
 * 
 * 定义编译器内部类型表示，用于类型检查和代码生成。
 */

#ifndef CDD_SEMANTIC_TYPE_H
#define CDD_SEMANTIC_TYPE_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace cdd {
namespace semantic {

// 前向声明
class Type;
using TypePtr = std::shared_ptr<Type>;

/**
 * @brief 类型种类
 */
enum class TypeKind {
    Void,
    Integer,
    Float,
    Pointer,
    Array,
    Function,
    Struct,
    Union,
    Enum,
};

/**
 * @brief 整数类型种类
 */
enum class IntegerKind {
    Char,
    Short,
    Int,
    Long,
    LongLong,
};

/**
 * @brief 浮点类型种类
 */
enum class FloatKind {
    Float,
    Double,
    LongDouble,
};

/**
 * @class Type
 * @brief 类型基类
 * 
 * 所有具体类型（整数、浮点、指针、数组等）的基类。
 * 提供类型大小、对齐、判断和字符串表示等公共接口。
 */
class Type {
public:
    TypeKind kind;
    bool isConst = false;
    bool isVolatile = false;
    bool isUnsigned = false;

    explicit Type(TypeKind k) : kind(k) {}
    virtual ~Type() = default;

    virtual int size() const = 0;
    virtual int alignment() const = 0;
    virtual std::string toString() const = 0;
    virtual TypePtr clone() const = 0;

    bool isVoid() const { return kind == TypeKind::Void; }
    bool isInteger() const { return kind == TypeKind::Integer || kind == TypeKind::Enum; }
    bool isFloat() const { return kind == TypeKind::Float; }
    bool isPointer() const { return kind == TypeKind::Pointer; }
    bool isArray() const { return kind == TypeKind::Array; }
    bool isFunction() const { return kind == TypeKind::Function; }
    bool isStruct() const { return kind == TypeKind::Struct; }
    bool isUnion() const { return kind == TypeKind::Union; }
    bool isEnum() const { return kind == TypeKind::Enum; }
    
    bool isArithmetic() const { return isInteger() || isFloat(); }
    bool isScalar() const { return isArithmetic() || isPointer(); }
    bool isAggregate() const { return isArray() || isStruct() || isUnion(); }
};

/**
 * @brief void 类型
 */
class VoidType : public Type {
public:
    VoidType() : Type(TypeKind::Void) {}

    int size() const override { return 0; }
    int alignment() const override { return 1; }
    std::string toString() const override { return "void"; }
    TypePtr clone() const override { return std::make_shared<VoidType>(*this); }
};

/**
 * @brief 整数类型
 */
class IntegerType : public Type {
public:
    IntegerKind intKind;

    explicit IntegerType(IntegerKind k, bool isUnsigned = false)
        : Type(TypeKind::Integer), intKind(k) {
        this->isUnsigned = isUnsigned;
    }

    int size() const override {
        switch (intKind) {
            case IntegerKind::Char: return 1;
            case IntegerKind::Short: return 2;
            case IntegerKind::Int: return 4;
            case IntegerKind::Long: return 8;      // LP64
            case IntegerKind::LongLong: return 8;
        }
        return 4;
    }

    int alignment() const override { return size(); }

    std::string toString() const override {
        std::string result;
        if (isUnsigned) result = "unsigned ";
        switch (intKind) {
            case IntegerKind::Char: result += "char"; break;
            case IntegerKind::Short: result += "short"; break;
            case IntegerKind::Int: result += "int"; break;
            case IntegerKind::Long: result += "long"; break;
            case IntegerKind::LongLong: result += "long long"; break;
        }
        return result;
    }

    TypePtr clone() const override { return std::make_shared<IntegerType>(*this); }
};

/**
 * @brief 浮点类型
 */
class FloatType : public Type {
public:
    FloatKind floatKind;

    explicit FloatType(FloatKind k) : Type(TypeKind::Float), floatKind(k) {}

    int size() const override {
        switch (floatKind) {
            case FloatKind::Float: return 4;
            case FloatKind::Double: return 8;
            case FloatKind::LongDouble: return 16;
        }
        return 8;
    }

    int alignment() const override { return size(); }

    std::string toString() const override {
        switch (floatKind) {
            case FloatKind::Float: return "float";
            case FloatKind::Double: return "double";
            case FloatKind::LongDouble: return "long double";
        }
        return "double";
    }

    TypePtr clone() const override { return std::make_shared<FloatType>(*this); }
};

/**
 * @brief 指针类型
 */
class PointerType : public Type {
public:
    TypePtr pointee;

    explicit PointerType(TypePtr base) : Type(TypeKind::Pointer), pointee(std::move(base)) {}

    int size() const override { return 8; }
    int alignment() const override { return 8; }

    std::string toString() const override {
        return pointee->toString() + "*";
    }

    TypePtr clone() const override {
        return std::make_shared<PointerType>(pointee->clone());
    }
};

/**
 * @brief 数组类型
 */
class ArrayType : public Type {
public:
    TypePtr elementType;
    int length;  // -1 表示不完整数组

    ArrayType(TypePtr elem, int len)
        : Type(TypeKind::Array), elementType(std::move(elem)), length(len) {}

    int size() const override {
        if (length < 0) return 0;
        return elementType->size() * length;
    }

    int alignment() const override { return elementType->alignment(); }

    std::string toString() const override {
        if (length < 0) {
            return elementType->toString() + "[]";
        }
        return elementType->toString() + "[" + std::to_string(length) + "]";
    }

    TypePtr clone() const override {
        return std::make_shared<ArrayType>(elementType->clone(), length);
    }
};

/**
 * @brief 函数类型
 */
class FunctionType : public Type {
public:
    TypePtr returnType;
    std::vector<TypePtr> paramTypes;
    std::vector<std::string> paramNames;
    bool isVariadic = false;

    explicit FunctionType(TypePtr ret)
        : Type(TypeKind::Function), returnType(std::move(ret)) {}

    int size() const override { return 0; }
    int alignment() const override { return 1; }

    std::string toString() const override {
        std::string result = returnType->toString() + "(";
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (i > 0) result += ", ";
            result += paramTypes[i]->toString();
        }
        if (isVariadic) {
            if (!paramTypes.empty()) result += ", ";
            result += "...";
        }
        result += ")";
        return result;
    }

    TypePtr clone() const override {
        auto ft = std::make_shared<FunctionType>(returnType->clone());
        for (const auto& pt : paramTypes) {
            ft->paramTypes.push_back(pt->clone());
        }
        ft->paramNames = paramNames;
        ft->isVariadic = isVariadic;
        return ft;
    }
};

/**
 * @brief 结构体/联合体成员
 */
struct Member {
    std::string name;
    TypePtr type;
    int offset;
};

/**
 * @brief 结构体类型
 */
class StructType : public Type {
public:
    std::string name;
    std::vector<Member> members;
    bool isComplete = false;

    explicit StructType(const std::string& n = "")
        : Type(TypeKind::Struct), name(n) {}

    int size() const override {
        if (!isComplete || members.empty()) return 0;
        const auto& last = members.back();
        int s = last.offset + last.type->size();
        // 对齐到最大成员对齐
        int align = alignment();
        return (s + align - 1) / align * align;
    }

    int alignment() const override {
        int maxAlign = 1;
        for (const auto& m : members) {
            maxAlign = std::max(maxAlign, m.type->alignment());
        }
        return maxAlign;
    }

    std::string toString() const override {
        return "struct " + (name.empty() ? "<anonymous>" : name);
    }

    TypePtr clone() const override {
        auto st = std::make_shared<StructType>(name);
        st->members = members;
        st->isComplete = isComplete;
        return st;
    }

    const Member* findMember(const std::string& memberName) const {
        for (const auto& m : members) {
            if (m.name == memberName) return &m;
        }
        return nullptr;
    }
};

/**
 * @brief 联合体类型
 */
class UnionType : public Type {
public:
    std::string name;
    std::vector<Member> members;
    bool isComplete = false;

    explicit UnionType(const std::string& n = "")
        : Type(TypeKind::Union), name(n) {}

    int size() const override {
        int maxSize = 0;
        for (const auto& m : members) {
            maxSize = std::max(maxSize, m.type->size());
        }
        return maxSize;
    }

    int alignment() const override {
        int maxAlign = 1;
        for (const auto& m : members) {
            maxAlign = std::max(maxAlign, m.type->alignment());
        }
        return maxAlign;
    }

    std::string toString() const override {
        return "union " + (name.empty() ? "<anonymous>" : name);
    }

    TypePtr clone() const override {
        auto ut = std::make_shared<UnionType>(name);
        ut->members = members;
        ut->isComplete = isComplete;
        return ut;
    }

    const Member* findMember(const std::string& memberName) const {
        for (const auto& m : members) {
            if (m.name == memberName) return &m;
        }
        return nullptr;
    }
};

/**
 * @brief 枚举类型
 */
class EnumType : public Type {
public:
    std::string name;
    std::unordered_map<std::string, int64_t> enumerators;

    explicit EnumType(const std::string& n = "")
        : Type(TypeKind::Enum), name(n) {}

    int size() const override { return 4; }  // 枚举存储为 int
    int alignment() const override { return 4; }

    std::string toString() const override {
        return "enum " + (name.empty() ? "<anonymous>" : name);
    }

    TypePtr clone() const override {
        auto et = std::make_shared<EnumType>(name);
        et->enumerators = enumerators;
        return et;
    }
};

//=============================================================================
// 类型工具函数
//=============================================================================

// 创建基本类型的工厂函数
inline TypePtr makeVoid() { return std::make_shared<VoidType>(); }
inline TypePtr makeChar(bool isUnsigned = false) { 
    return std::make_shared<IntegerType>(IntegerKind::Char, isUnsigned); 
}
inline TypePtr makeShort(bool isUnsigned = false) { 
    return std::make_shared<IntegerType>(IntegerKind::Short, isUnsigned); 
}
inline TypePtr makeInt(bool isUnsigned = false) { 
    return std::make_shared<IntegerType>(IntegerKind::Int, isUnsigned); 
}
inline TypePtr makeLong(bool isUnsigned = false) { 
    return std::make_shared<IntegerType>(IntegerKind::Long, isUnsigned); 
}
inline TypePtr makeLongLong(bool isUnsigned = false) { 
    return std::make_shared<IntegerType>(IntegerKind::LongLong, isUnsigned); 
}
inline TypePtr makeFloat() { return std::make_shared<FloatType>(FloatKind::Float); }
inline TypePtr makeDouble() { return std::make_shared<FloatType>(FloatKind::Double); }
inline TypePtr makePointer(TypePtr base) { return std::make_shared<PointerType>(std::move(base)); }
inline TypePtr makeArray(TypePtr elem, int len) { 
    return std::make_shared<ArrayType>(std::move(elem), len); 
}

// 类型兼容性检查
bool areTypesCompatible(const TypePtr& t1, const TypePtr& t2);
bool canImplicitlyConvert(const TypePtr& from, const TypePtr& to);

// 类型提升
TypePtr getCommonType(const TypePtr& t1, const TypePtr& t2);

} // namespace semantic
} // namespace cdd

#endif // CDD_SEMANTIC_TYPE_H
