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

std::size_t length(std::int64_t count, std::string_view name = "count");
std::span<const Index> listed(int count, const int *indices, std::string_view name = "indices");
std::vector<Offset> offsets(std::size_t count, const std::int64_t *starts);
std::vector<LinearTerm> packed(std::size_t size, const int *variables, const double *values);
void unpack(std::span<const LinearTerm> terms, std::size_t size, int *variables, double *values);

template <typename Read> void write_starts(std::size_t count, std::int64_t *starts, Read read)
{
    if (starts == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j <= count; ++j)
    {
        starts[j] = static_cast<std::int64_t>(read(static_cast<Index>(j)).size());
    }
}

}
