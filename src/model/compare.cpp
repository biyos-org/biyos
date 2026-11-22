#include "model/compare.hpp"

#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/terms.hpp"
#include "model/model.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

template <typename T> std::string against(std::string_view field, T a, T b)
{
    return std::string(field) + " " + text_of(a) + " against " + text_of(b);
}

std::string compared(std::string_view field, Index a, Index b)
{
    return a == b ? std::string() : against(field, a, b);
}

std::string compared(std::string_view field, double a, double b)
{
    const bool same = std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
    return same ? std::string() : against(field, a, b);
}

std::string quoted(const std::string *name)
{
    return name == nullptr ? "none" : "'" + *name + "'";
}

std::string compared(std::string_view field, const std::string *a, const std::string *b)
{
    const bool same = a == nullptr || b == nullptr ? a == b : *a == *b;
    return same ? std::string() : std::string(field) + " " + quoted(a) + " against " + quoted(b);
}

std::string first_of(std::initializer_list<std::string> found)
{
    for (const std::string &text : found)
    {
        if (!text.empty())
        {
            return text;
        }
    }
    return "";
}

template <typename Count, typename Compare>
std::string table_difference(std::string_view noun, Count a, Count b, Compare compare)
{
    if (a != b)
    {
        return against(std::string(noun) + " count", a, b);
    }
    for (Count k = 0; k < a; ++k)
    {
        const std::string found = compare(k);
        if (!found.empty())
        {
            return std::string(noun) + " " + text_of(k) + ": " + found;
        }
    }
    return "";
}

int kind_of(const Variable &var)
{
    return static_cast<int>(var.kind);
}

std::string differ(const Variable &a, const Variable &b)
{
    return first_of({
        compared("kind", kind_of(a), kind_of(b)),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

std::string differ(const LinearTerm &a, const LinearTerm &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("coefficient", a.coefficient, b.coefficient),
    });
}

std::string differ(const QuadraticTerm &a, const QuadraticTerm &b)
{
    return first_of({
        compared("first variable", a.row, b.row),
        compared("second variable", a.column, b.column),
        compared("coefficient", a.coefficient, b.coefficient),
    });
}

std::string differ(const Constraint &a, const Constraint &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

int sense_of(const Objective &obj)
{
    return static_cast<int>(obj.sense);
}

std::string differ(const Objective &a, const Objective &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("sense", sense_of(a), sense_of(b)),
        compared("priority", a.priority, b.priority),
        compared("weight", a.weight, b.weight),
        compared("absolute tolerance", a.absolute, b.absolute),
        compared("relative tolerance", a.relative, b.relative),
    });
}

template <typename T> Offset size_of(std::span<const T> terms)
{
    return static_cast<Offset>(terms.size());
}

template <typename T> const T &at(std::span<const T> terms, Offset k)
{
    return terms[static_cast<std::size_t>(k)];
}

template <typename T>
std::string terms_difference(std::string_view noun, std::span<const T> a, std::span<const T> b)
{
    const auto term = [&](Offset k) { return differ(at(a, k), at(b, k)); };
    return table_difference(noun, size_of(a), size_of(b), term);
}

double constant_of(const Model &mdl, Index fun)
{
    return mdl.constants(fun, 1).front();
}

std::span<const QuadraticTerm> quad_of(const Model &mdl, Index fun)
{
    return mdl.quad_terms(fun, 1);
}

std::string named(const Model &a, const Model &b, Family family, Index idx)
{
    return compared("name", a.name_of(family, idx), b.name_of(family, idx));
}

std::string var_difference(const Model &a, const Model &b, Index var)
{
    return first_of({
        differ(a.variable(var), b.variable(var)),
        named(a, b, Family::Variable, var),
    });
}

std::string fun_difference(const Model &a, const Model &b, Index fun)
{
    return first_of({
        compared("constant", constant_of(a, fun), constant_of(b, fun)),
        terms_difference("term", a.terms(fun, 1), b.terms(fun, 1)),
        terms_difference("quadratic term", quad_of(a, fun), quad_of(b, fun)),
        named(a, b, Family::Function, fun),
    });
}

std::string con_difference(const Model &a, const Model &b, Index con)
{
    return first_of({
        differ(a.constraint(con), b.constraint(con)),
        named(a, b, Family::Constraint, con),
    });
}

std::string obj_difference(const Model &a, const Model &b, Index obj)
{
    return first_of({
        differ(a.objective(obj), b.objective(obj)),
        named(a, b, Family::Objective, obj),
    });
}

std::string vars_difference(const Model &a, const Model &b)
{
    const auto var = [&](Index k) { return var_difference(a, b, k); };
    return table_difference("variable", a.n_vars(), b.n_vars(), var);
}

std::string funs_difference(const Model &a, const Model &b)
{
    const auto fun = [&](Index k) { return fun_difference(a, b, k); };
    return table_difference("function", a.n_funs(), b.n_funs(), fun);
}

std::string cons_difference(const Model &a, const Model &b)
{
    const auto con = [&](Index k) { return con_difference(a, b, k); };
    return table_difference("constraint", a.n_cons(), b.n_cons(), con);
}

std::string objs_difference(const Model &a, const Model &b)
{
    const auto obj = [&](Index k) { return obj_difference(a, b, k); };
    return table_difference("objective", a.n_objs(), b.n_objs(), obj);
}

}

std::string difference(const Model &a, const Model &b)
{
    std::string found = compared("model name", a.name(), b.name());
    if (found.empty())
    {
        found = vars_difference(a, b);
    }
    if (found.empty())
    {
        found = funs_difference(a, b);
    }
    if (found.empty())
    {
        found = cons_difference(a, b);
    }
    if (found.empty())
    {
        found = objs_difference(a, b);
    }
    return found;
}

}
