#pragma once

#include "core/options.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

std::span<const OptionSpec> opt_table();
std::size_t idx_of(Option opt);
const OptionSpec &row_of(std::span<const OptionSpec> table, Option opt);

}
