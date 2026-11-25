#pragma once

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "model/attributes/records.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

template <typename T> std::string against(std::string_view field, T a, T b)
{
    return std::string(field) + " " + text_of(a) + " against " + text_of(b);
}

std::string compared(std::string_view field, Index a, Index b);
std::string compared(std::string_view field, Offset a, Offset b);
std::string compared(std::string_view field, double a, double b);
std::string compared(std::string_view field, const std::string *a, const std::string *b);
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
std::string differ(Index a, Index b);
std::string differ(double a, double b);
std::string differ(const Tolerance &a, const Tolerance &b);
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

template <typename T> Offset size_of(std::span<const T> terms)
{
    return to_offset(terms.size());
}

template <typename T> const T &at(std::span<const T> terms, Offset k)
{
    return terms[to_size(k)];
}

template <typename T>
std::string term_differences(std::string_view noun, std::span<const T> a, std::span<const T> b)
{
    const auto term = [&](Offset k) { return differ(at(a, k), at(b, k)); };
    return table_difference(noun, size_of(a), size_of(b), term);
}

}
