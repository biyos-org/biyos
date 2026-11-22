#pragma once

#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace biyos::capi
{

std::size_t length(std::int64_t n, std::string_view name = "n");
std::span<const Index> listed(int n, const int *indices, std::string_view name = "idx");
std::vector<Offset> offsets(std::size_t n, const std::int64_t *starts);
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

template <typename Read> void write_starts(std::size_t n, std::int64_t *starts, Read read)
{
    if (starts == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j <= n; ++j)
    {
        starts[j] = static_cast<std::int64_t>(read(static_cast<Index>(j)).size());
    }
}

}
