#pragma once

#include "core/numbers.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace biyos::capi
{

template <typename Term> struct Grouped
{
    std::vector<Index> functions;
    std::vector<Offset> starts;
    std::vector<Term> terms;
};

template <typename Term>
Grouped<Term> grouped(std::span<const Index> functions, std::span<const Term> terms)
{
    std::vector<std::size_t> order(terms.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, {}, [&](std::size_t k) { return functions[k]; });
    Grouped<Term> found;
    found.terms.reserve(terms.size());
    for (const std::size_t k : order)
    {
        if (found.functions.empty() || found.functions.back() != functions[k])
        {
            found.functions.push_back(functions[k]);
            found.starts.push_back(to_offset(found.terms.size()));
        }
        found.terms.push_back(terms[k]);
    }
    found.starts.push_back(to_offset(found.terms.size()));
    return found;
}

inline std::vector<Index> owners_of(std::span<const Index> records, std::span<const Offset> starts)
{
    std::vector<Index> owners;
    owners.reserve(to_size(starts.back()));
    for (std::size_t j = 0; j < records.size(); ++j)
    {
        owners.insert(owners.end(), to_size(starts[j + 1] - starts[j]), records[j]);
    }
    return owners;
}

}
