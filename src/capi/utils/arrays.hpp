#pragma once

#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos::capi
{

std::size_t length(BIYOS_offset n, std::string_view name = "n");
void check_capacity(BIYOS_offset capacity,
                    std::size_t needed,
                    std::string_view name = "cap",
                    std::string_view items = "terms");

template <typename T>
std::span<const T> listed(BIYOS_index n, const T *items, std::string_view name = "idx")
{
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(items, name);
    }
    return {items, size};
}

std::vector<Offset> starts_of(std::size_t n,
                              const BIYOS_offset *counts,
                              std::string_view name = "cnt");
void write_counts(std::span<const Offset> starts, BIYOS_offset *counts);
std::vector<LinearTerm> packed(std::size_t size,
                               const BIYOS_index *variables,
                               const BIYOS_real *values);
std::vector<QuadraticTerm> packed(std::size_t size,
                                  const BIYOS_index *rows,
                                  const BIYOS_index *columns,
                                  const BIYOS_real *values);

template <typename T> std::span<T> given(T *array, std::size_t size)
{
    return array == nullptr ? std::span<T>() : std::span<T>(array, size);
}

template <typename T> T entry_or(const T *values, std::size_t k, T held)
{
    return values == nullptr ? held : values[k];
}

template <typename T> void put(T *values, std::size_t k, T value)
{
    if (values != nullptr)
    {
        values[k] = value;
    }
}

template <typename Record>
std::vector<Record> picked(const Model &model,
                           const Record &(Model::*read)(Index) const,
                           std::span<const Index> indices)
{
    std::vector<Record> found;
    found.reserve(indices.size());
    for (const Index i : indices)
    {
        found.push_back((model.*read)(i));
    }
    return found;
}

template <typename Read> void write_counts(std::size_t n, BIYOS_offset *counts, Read read)
{
    if (counts == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j < n; ++j)
    {
        counts[j] = to_offset(read(to_index(j)).size());
    }
}
}
