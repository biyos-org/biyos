#pragma once

#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace biyos::capi
{

std::size_t length(BIYOS_offset n, std::string_view name = "n");
void check_capacity(BIYOS_offset capacity,
                    std::size_t needed,
                    std::string_view name = "cap",
                    std::string_view items = "terms");

template <typename T> std::span<T> span_of(T *items, std::size_t size, std::string_view name)
{
    if (size > 0)
    {
        require(items, name);
    }
    return {items, size};
}

template <typename T>
std::span<const T> listed(BIYOS_index n,
                          const T *items,
                          std::string_view name,
                          std::string_view count = "n")
{
    return span_of(items, length(n, count), name);
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

template <typename T> void write_array(std::span<const std::type_identity_t<T>> from, T *to)
{
    if (to != nullptr)
    {
        std::ranges::copy(from, to);
    }
}

template <typename T> void write_value(std::type_identity_t<T> value, T *to)
{
    if (to != nullptr)
    {
        *to = value;
    }
}

template <typename Field, typename T>
concept field_of = std::same_as<Field, T> || std::is_enum_v<Field>;

template <typename Record, typename Field, typename T>
    requires field_of<Field, T>
void write_field(std::span<const std::type_identity_t<Record>> records, Field Record::*field, T *to)
{
    if (to == nullptr)
    {
        return;
    }
    for (std::size_t k = 0; k < records.size(); ++k)
    {
        to[k] = static_cast<T>(records[k].*field);
    }
}

template <typename Record, typename Field, typename T>
    requires field_of<Field, T>
void read_field(std::span<std::type_identity_t<Record>> records,
                Field Record::*field,
                const T *from)
{
    if (from == nullptr)
    {
        return;
    }
    for (std::size_t k = 0; k < records.size(); ++k)
    {
        records[k].*field = static_cast<Field>(from[k]);
    }
}

template <typename Kind> std::vector<Kind> kinds_of(std::span<const BIYOS_int> codes)
{
    std::vector<Kind> kinds;
    kinds.reserve(codes.size());
    for (const BIYOS_int code : codes)
    {
        kinds.push_back(static_cast<Kind>(code));
    }
    return kinds;
}

template <typename Kind> void write_codes(const std::vector<Kind> &kinds, BIYOS_int *codes)
{
    if (codes == nullptr)
    {
        return;
    }
    for (std::size_t k = 0; k < kinds.size(); ++k)
    {
        codes[k] = static_cast<BIYOS_int>(kinds[k]);
    }
}
}
