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
std::span<const Index> listed(int count, const int *indices);
std::vector<LinearTerm> packed(std::size_t size, const int *variables, const double *values);
void unpack(std::span<const LinearTerm> terms, std::size_t size, int *variables, double *values);

}
