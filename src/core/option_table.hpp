#pragma once

#include "core/options.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

std::span<const OptionSpec> option_table();
std::size_t index_of(Option option);
const OptionSpec &row_of(std::span<const OptionSpec> table, Option option);

}
