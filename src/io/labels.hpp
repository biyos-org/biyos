#pragma once

#include "core/index.hpp"
#include "model/model.hpp"

#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_set>

namespace biyos
{

using Taken = std::unordered_set<std::string_view>;

Taken user_names(const Model &model, std::initializer_list<Family> families);
std::string fresh(const std::string &base, const Taken &taken);
std::string label_of(const Model &model,
                     Family family,
                     Index index,
                     const std::string &base,
                     const Taken &taken);

}
