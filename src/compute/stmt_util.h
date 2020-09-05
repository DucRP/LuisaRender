//
// Created by Mike Smith on 2020/7/30.
//

#pragma once

#include <compute/expr_util.h>
#include <compute/function.h>

namespace luisa::compute::dsl {

inline void void_(Variable v) { Function::current().add_statement(std::make_unique<ExprStmt>(v)); }

#define MAKE_VARIABLE_ASSIGNMENT_OPERATOR_IMPL(op)              \
    template<typename T, detail::EnableIfLiteralOperand<T>>     \
    inline void Variable::operator op(T &&rhs) const noexcept { \
        this->operator op(literal(std::forward<T>(rhs)));       \
    }

LUISA_MAP(MAKE_VARIABLE_ASSIGNMENT_OPERATOR_IMPL, =, +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=)
#undef MAKE_VARIABLE_ASSIGNMENT_OPERATOR_IMPL

template<typename T>
struct Arg : public Variable, Noncopyable {
    
    template<typename U>
    explicit Arg(const BufferView<U> &bv) noexcept : Variable{Function::current().arg<T>(std::move(bv))} {}
    
    explicit Arg(std::shared_ptr<Texture> tex) noexcept: Variable{Function::current().arg<T>(std::move(tex))} {}
    explicit Arg(Texture *tex) noexcept: Variable{Function::current().arg<T>(tex->shared_from_this())} {}
    explicit Arg(Texture &tex) noexcept: Variable{Function::current().arg<T>(tex.shared_from_this())} {}
    
    // For embedding immutable uniform data
    template<typename U, std::enable_if_t<std::negation_v<std::is_pointer<U>>, int> = 0>
    explicit Arg(U data) noexcept : Variable{Function::current().arg<T>(&data, sizeof(data))} {}
    
    // For binding mutable uniform data
    explicit Arg(void *p) noexcept: Variable{Function::current().arg<T>(p)} {}
};

template<typename T>
struct Var : public Variable {
    
    template<typename... Literals>
    Var(Literals &&... vs) noexcept : Variable{Function::current().var<T>(std::forward<Literals>(vs)...)} {}
};

#define LUISA_STRUCT_BEGIN(S)                                                                    \
namespace luisa::compute::dsl {                                                                  \
    template<>                                                                                   \
    struct Structure<S> {                                                                        \
        [[nodiscard]] static TypeDesc *desc() noexcept {                                         \
            using This = S;                                                                      \
            static TypeDesc td;                                                                  \
            static int depth = 0;                                                                \
            if (depth++ == 0) {                                                                  \
                td.type = TypeCatalog::STRUCTURE;                                                \
                td.member_names.clear();                                                         \
                td.member_types.clear();                                                         \

#define LUISA_STRUCT_MEMBER(member)                                                              \
                td.member_names.emplace_back(#member);                                           \
                td.member_types.emplace_back(type_desc<decltype(std::declval<This>().member)>);  \

#define LUISA_STRUCT_END()                                                                       \
            }                                                                                    \
            depth--;                                                                             \
            return &td;                                                                          \
        }                                                                                        \
    };                                                                                           \
}                                                                                                \

#define LUISA_STRUCT_MEMBER_TO_VAR(name) \
Variable name() const noexcept { return member(#name); }

#define LUISA_STRUCT_VAR_SPECIALIZE(S, ...)                                                                              \
namespace luisa::compute::dsl {                                                                                          \
    template<>                                                                                                           \
    struct Var<S> : Variable {                                                                                           \
        using This = S;                                                                                                  \
        template<typename... Literals>                                                                                   \
        Var(Literals &&... vs) noexcept : Variable{Function::current().var<This>(std::forward<Literals>(vs)...)} {}      \
        LUISA_MAP(LUISA_STRUCT_MEMBER_TO_VAR, __VA_ARGS__)                                                               \
    };                                                                                                                   \
    template<>                                                                                                           \
    struct Arg<S> : public Variable, Noncopyable {                                                                       \
        using This = S;                                                                                                  \
        template<typename U>                                                                                             \
        explicit Arg(const BufferView<U> &bv) noexcept : Variable{Function::current().arg<This>(std::move(bv))} {}       \
        explicit Arg(std::shared_ptr<Texture> tex) noexcept: Variable{Function::current().arg<This>(std::move(tex))} {}  \
        explicit Arg(Texture *tex) noexcept: Variable{Function::current().arg<This>(tex->shared_from_this())} {}         \
        explicit Arg(Texture &tex) noexcept: Variable{Function::current().arg<This>(tex.shared_from_this())} {}          \
        template<typename U, std::enable_if_t<std::negation_v<std::is_pointer<U>>, int> = 0>                             \
        explicit Arg(U data) noexcept : Variable{Function::current().arg<This>(&data, sizeof(data))} {}                  \
        explicit Arg(void *p) noexcept: Variable{Function::current().arg<This>(p)} {}                                    \
        LUISA_MAP(LUISA_STRUCT_MEMBER_TO_VAR, __VA_ARGS__)                                                               \
    };                                                                                                                   \
}

#define LUISA_STRUCT(S, ...)                            \
LUISA_STRUCT_BEGIN(S)                                   \
     LUISA_MAP(LUISA_STRUCT_MEMBER, __VA_ARGS__)        \
LUISA_STRUCT_END()                                      \
LUISA_STRUCT_VAR_SPECIALIZE(S, __VA_ARGS__)             \

template<typename T>
struct Threadgroup : public Variable {
    Threadgroup() noexcept: Variable{Function::current().threadgroup_var<T>()} {}
};

template<typename T>
struct Let : public Variable {
    
    template<typename... Literals>
    explicit Let(Literals &&... vs) noexcept : Variable{Function::current().constant<T>(std::forward<Literals>(vs)...)} {}
};

// Used for arguments passed by value
template<typename T>
using Expr = Variable;

struct IfStmtBuilder {
    
    explicit IfStmtBuilder(Variable cond) noexcept { Function::current().add_statement(std::make_unique<IfStmt>(std::move(cond))); }
    
    template<typename True, std::enable_if_t<std::is_invocable_v<True>, int> = 0>
    const IfStmtBuilder &operator<<(True &&t) const noexcept {
        Function::current().block(std::forward<True>(t));
        return *this;
    }
    
    template<typename False, std::enable_if_t<std::is_invocable_v<False>, int> = 0>
    void operator>>(False &&f) const noexcept {
        Function::current().add_statement(std::make_unique<KeywordStmt>("else"));
        Function::current().block(std::forward<False>(f));
    }
    
    // for else if
    const IfStmtBuilder &operator>>(Variable else_cond) const noexcept {
        Function::current().add_statement(std::make_unique<KeywordStmt>("else"));
        Function::current().add_statement(std::make_unique<IfStmt>(std::move(else_cond), true));
        return *this;
    }
};

struct SwitchStmtBuilder {
    
    explicit SwitchStmtBuilder(Variable expr) noexcept { Function::current().add_statement(std::make_unique<SwitchStmt>(std::move(expr))); }
    
    template<typename Body, std::enable_if_t<std::is_invocable_v<Body>, int> = 0>
    void operator<<(Body &&body) const noexcept {
        Function::current().block(std::forward<Body>(body));
    }
};

struct CaseStmtBuilder {
    
    explicit CaseStmtBuilder(Variable expr) noexcept { Function::current().add_statement(std::make_unique<CaseStmt>(std::move(expr))); }
    CaseStmtBuilder() noexcept { Function::current().add_statement(std::make_unique<CaseStmt>()); }
    
    template<typename Body, std::enable_if_t<std::is_invocable_v<Body>, int> = 0>
    void operator<<(Body &&body) const noexcept {
        Function::current().block(std::forward<Body>(body));
    }
};

struct WhileStmtBuilder {
    
    explicit WhileStmtBuilder(Variable cond) noexcept { Function::current().add_statement(std::make_unique<WhileStmt>(std::move(cond))); }
    
    template<typename Body, std::enable_if_t<std::is_invocable_v<Body>, int> = 0>
    void operator<<(Body &&body) const noexcept {
        Function::current().block(std::forward<Body>(body));
    }
};

struct LoopWhenStmtBuilder {
    
    LoopWhenStmtBuilder() noexcept { Function::current().add_statement(std::make_unique<KeywordStmt>("do")); }
    
    template<typename Body, std::enable_if_t<std::is_invocable_v<Body>, int> = 0>
    const LoopWhenStmtBuilder &operator<<(Body &&body) const noexcept {
        Function::current().block(std::forward<Body>(body));
        return *this;
    }
    
    void operator>>(Variable cond) const noexcept {
        Function::current().add_statement(std::make_unique<WhileStmt>(std::move(cond), true));
    }
};

class ForStmtBuilder {

private:
    Variable _i;

public:
    template<typename Begin, typename End, typename Step>
    ForStmtBuilder(Begin &&begin, End &&end, Step &&step) noexcept
        : _i{Function::current().var<AutoType>(std::forward<Begin>(begin))} {
        Function::current().add_statement(std::make_unique<ForStmt>(_i, literal(std::forward<End>(end)), literal(std::forward<Step>(step))));
    }
    
    template<typename Begin, typename End>
    ForStmtBuilder(Begin &&begin, End &&end) noexcept
        : ForStmtBuilder(std::forward<Begin>(begin), std::forward<End>(end), literal(1)) {}
    
    template<typename Body, std::enable_if_t<std::is_invocable_v<Body, Variable>, int> = 0>
    void operator<<(Body &&body) const noexcept {
        Function::current().block([&] { body(_i); });
    }
};

using Auto = Var<AutoType>;

#define MAKE_CONVENIENT_VARIABLE_TYPE(VarType, Type) \
    using VarType = Var<Type>;                       \
    using VarType##2 = Var<Type##2>;                 \
    using VarType##3 = Var<Type##3>;                 \
    using VarType##4 = Var<Type##4>;                 \
    using Packed##VarType##3 = Var<packed_##Type##3>;

MAKE_CONVENIENT_VARIABLE_TYPE(Bool, bool)
MAKE_CONVENIENT_VARIABLE_TYPE(Float, float)
MAKE_CONVENIENT_VARIABLE_TYPE(Char, char)
MAKE_CONVENIENT_VARIABLE_TYPE(UChar, uchar)
MAKE_CONVENIENT_VARIABLE_TYPE(Short, short)
MAKE_CONVENIENT_VARIABLE_TYPE(UShort, ushort)
MAKE_CONVENIENT_VARIABLE_TYPE(Int, int)
MAKE_CONVENIENT_VARIABLE_TYPE(UInt, uint)

#undef MAKE_CONVENIENT_VARIABLE_TYPE

// For highlighting...
#define Arg Arg
#define Var Var
#define Let Let
#define Expr Expr
#define Threadgroup Threadgroup

#define Bool Bool
#define Float Float
#define Int8 Int8
#define UInt8 UInt8
#define Int16 Int16
#define UInt16 UInt16
#define Int32 Int32
#define UInt32 UInt32
#define Int64 Int64
#define UInt64 UInt64
#define Auto Auto

#define If(...) IfStmtBuilder{literal(__VA_ARGS__)} << [&]
#define Else >> [&]
#define Elif(...) >> literal(__VA_ARGS__) << [&]

#define Switch(...) SwitchStmtBuilder{literal(__VA_ARGS__)} << [&]
#define Case(...) CaseStmtBuilder{literal(__VA_ARGS__)} << [&]
#define Default CaseStmtBuilder{} << [&]

#define While(...) WhileStmtBuilder{literal(__VA_ARGS__)} << [&]

#define Loop LoopWhenStmtBuilder{} << [&]
#define When(...) >> literal(__VA_ARGS__)

#define For(v, ...) ForStmtBuilder{__VA_ARGS__} << [&](const Variable &v)

inline void break_() noexcept { Function::current().add_break(); }
inline void continue_() noexcept { Function::current().add_continue(); }
inline void return_() noexcept { Function::current().add_return(); }

#define Break break_()
#define Continue continue_()
#define Return return_()

}// namespace luisa::compute::dsl
