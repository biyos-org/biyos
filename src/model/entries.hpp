#pragma once

#include "core/index.hpp"
#include "model/model.hpp"

#include <span>
#include <string>

namespace biyos
{

void check_entries(const std::string &object, std::span<const Entry> entries, Index variables);
void sort_entries(const std::string &object, std::span<Entry> entries);

}
