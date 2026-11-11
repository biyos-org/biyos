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

int kind_of(const Variable &variable)
{
    return static_cast<int>(variable.kind);
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
        compared("first variable", a.first, b.first),
        compared("second variable", a.second, b.second),
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

double constant_of(const Model &model, Index function)
{
    return model.constants(function, 1).front();
}

std::span<const QuadraticTerm> quadratic_of(const Model &model, Index function)
{
    return model.quadratic_terms(function, 1);
}

std::string function_difference(const Model &a, const Model &b, Index function)
{
    return first_of({
        compared("constant", constant_of(a, function), constant_of(b, function)),
        terms_difference("term", a.terms(function, 1), b.terms(function, 1)),
        terms_difference("quadratic term", quadratic_of(a, function), quadratic_of(b, function)),
    });
}

std::string variables_difference(const Model &a, const Model &b)
{
    const auto variable = [&](Index k) { return differ(a.variable(k), b.variable(k)); };
    return table_difference("variable", a.variable_count(), b.variable_count(), variable);
}

std::string functions_difference(const Model &a, const Model &b)
{
    const auto function = [&](Index k) { return function_difference(a, b, k); };
    return table_difference("function", a.function_count(), b.function_count(), function);
}

std::string constraints_difference(const Model &a, const Model &b)
{
    const auto constraint = [&](Index k) { return differ(a.constraint(k), b.constraint(k)); };
    return table_difference("constraint", a.constraint_count(), b.constraint_count(), constraint);
}

}

std::string difference(const Model &a, const Model &b)
{
    std::string found = variables_difference(a, b);
    if (found.empty())
    {
        found = functions_difference(a, b);
    }
    if (found.empty())
    {
        found = constraints_difference(a, b);
    }
    return found;
}

}
