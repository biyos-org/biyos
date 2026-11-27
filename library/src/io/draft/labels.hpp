#pragma once

#include "core/numbers.hpp"
#include "io/utils/directory.hpp"
#include "model/model.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace biyos
{

using Taken = std::unordered_set<std::string_view>;

Taken user_names(const Model &model, std::initializer_list<Family> families);
std::string fresh(const std::string &base, const Taken &taken);
void enter(Directory &directory, const std::string &label, Family family, Index index);
std::optional<std::string> kept_label(const Model &model, Family family, Index index);
std::string label_of(const Model &model,
                     Family family,
                     Index index,
                     const std::string &base,
                     const Taken &taken);

}
