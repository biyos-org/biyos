#pragma once

#include "biyos/biyos.h"
#include "core/env.hpp"
#include "model/model.hpp"

#include <array>
#include <string_view>

namespace biyos
{

enum class Format
{
    Mps = BIYOS_FMT_MPS,
};

inline constexpr std::array kFormats{
    Format::Mps,
};

Format find_format(int id);
Model read_file(Env &env, std::string_view path, Format format);

}
