#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <string_view>

namespace biyos
{

[[noreturn]] void refuse_part(std::string_view format,
                              std::string_view noun,
                              Index index,
                              std::string_view part);
void check_absent(std::string_view format,
                  const Model &model,
                  Family family,
                  std::string_view part);
void check_weight(std::string_view format, const Model &model, Index objective);

}
