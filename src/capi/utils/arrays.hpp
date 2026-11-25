#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace biyos::capi
{

std::size_t length(std::int64_t n, std::string_view name = "n");
void check_capacity(std::int64_t capacity,
                    std::size_t needed,
                    std::string_view name = "cap",
                    std::string_view items = "terms");
std::span<const Index> listed(int n, const int *indices, std::string_view name = "idx");
std::vector<Offset> starts_of(std::size_t n,
                              const std::int64_t *counts,
                              std::string_view name = "cnt");
void write_counts(std::span<const Offset> starts, std::int64_t *counts);
std::vector<LinearTerm> packed(std::size_t size, const int *variables, const double *values);
void unpack(std::span<const LinearTerm> terms,
            std::size_t capacity,
            int *variables,
            double *values);
std::vector<QuadraticTerm> packed(std::size_t size,
                                  const int *rows,
                                  const int *columns,
                                  const double *values);
void unpack(std::span<const QuadraticTerm> terms,
            std::size_t capacity,
            int *rows,
            int *columns,
            double *values);

struct Coordinates
{
    int *functions;
    int *variables;
    double *values;
};

void put_term(const Coordinates &out, std::size_t k, Index function, const LinearTerm &term);

template <typename T> std::span<T> given(T *array, std::size_t size)
{
    return array == nullptr ? std::span<T>() : std::span<T>(array, size);
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

template <typename Read> void write_counts(std::size_t n, std::int64_t *counts, Read read)
{
    if (counts == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j < n; ++j)
    {
        counts[j] = static_cast<std::int64_t>(read(static_cast<Index>(j)).size());
    }
}
}
