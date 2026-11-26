#pragma once

#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/events/stream.hpp"
#include "core/options/settings.hpp"
#include "model/model.hpp"

#include <array>
#include <string_view>

namespace biyos
{

enum class FileFormat
{
    Mps = BIYOS_FILEFMT_MPS,
    Nl = BIYOS_FILEFMT_NL,
};

inline constexpr std::array kFormats{
    FileFormat::Mps,
    FileFormat::Nl,
};

FileFormat find_format(int id);
Model read_file(Env &env, std::string_view path, FileFormat format);
void write_file(Stream stream,
                const Model &model,
                const Settings &settings,
                std::string_view path,
                FileFormat format);

}
