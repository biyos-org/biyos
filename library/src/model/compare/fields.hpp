#pragma once

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/points.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"
#include "model/parts/attributes/records.hpp"

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace biyos
{

template <typename T> std::string against(std::string_view field, T a, T b)
{
    return std::string(field) + " " + text_of(a) + " against " + text_of(b);
}

std::string compared(std::string_view field, Index a, Index b);
std::string compared(std::string_view field, Offset a, Offset b);
std::string compared(std::string_view field, Real a, Real b);
std::string compared(std::string_view field, const std::string *a, const std::string *b);

template <typename T>
    requires std::is_enum_v<T>
std::string compared(std::string_view field, T a, T b)
{
    return a == b ? std::string() : against(field, static_cast<int>(a), static_cast<int>(b));
}

std::string first_of(std::initializer_list<std::string> found);

std::string differ(const Variable &a, const Variable &b);
std::string differ(const LinearTerm &a, const LinearTerm &b);
std::string differ(const QuadraticTerm &a, const QuadraticTerm &b);
std::string differ(const IndexPair &a, const IndexPair &b);
std::string differ(const Constraint &a, const Constraint &b);
std::string differ(const Objective &a, const Objective &b);
std::string differ(const Indicator &a, const Indicator &b);
std::string differ(const Complement &a, const Complement &b);
std::string differ(const Member &a, const Member &b);
std::string differ(const Tolerance &a, const Tolerance &b);
std::string differ(const PiecewiseLinear &a, const PiecewiseLinear &b);
std::string differ(const Callback &a, const Callback &b);

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

template <typename T, typename Compare>
std::string term_table(std::string_view noun,
                       std::span<const T> a,
                       std::span<const T> b,
                       Compare compare)
{
    const auto term = [&](Offset k) { return compare(a[to_size(k)], b[to_size(k)]); };
    return table_difference(noun, to_offset(a.size()), to_offset(b.size()), term);
}

template <typename T>
std::string term_table(std::string_view noun, std::span<const T> a, std::span<const T> b)
{
    const auto term = [](const T &left, const T &right) { return differ(left, right); };
    return term_table(noun, a, b, term);
}

template <typename T>
std::string value_table(std::string_view noun,
                        std::string_view field,
                        std::span<const T> a,
                        std::span<const T> b)
{
    const auto value = [&](T left, T right) { return compared(field, left, right); };
    return term_table(noun, a, b, value);
}

}
