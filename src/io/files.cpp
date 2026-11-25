#include "io/files.hpp"

#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/hooks.hpp"
#include "core/options/settings.hpp"
#include "core/timing/timer.hpp"
#include "io/draft/draft.hpp"
#include "io/draft/sketch.hpp"
#include "io/leftovers.hpp"
#include "io/mps/mps.hpp"
#include "io/mps/print.hpp"
#include "model/model.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>

namespace biyos
{

namespace
{

std::filesystem::path path_of(std::string_view path)
{
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

[[noreturn]] void refuse(const std::string &file, std::string_view reason)
{
    throw Error(ErrorKind::FileAccess, "file '" + file + "'", std::string(reason));
}

void print_file(const Draft &draft, const std::string &file, std::string_view path)
{
    const std::filesystem::path location = path_of(path);
    std::error_code code;
    if (std::filesystem::is_directory(location, code))
    {
        refuse(file, "is a directory");
    }
    std::ofstream output(location, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        refuse(file, "cannot be opened");
    }
    print_mps(draft, output);
    output.close();
    if (output.fail())
    {
        refuse(file, "cannot be written");
    }
}

}

FileFormat find_format(int id)
{
    for (const FileFormat format : kFormats)
    {
        if (static_cast<int>(format) == id)
        {
            return format;
        }
    }
    throw Error(ErrorKind::InvalidArgument, "format " + std::to_string(id), "unknown");
}

Model read_file(Env &env, std::string_view path, FileFormat format)
{
    const EntryTimer timer(env, "io", "read");
    const std::string file(path);
    const std::filesystem::path location = path_of(path);
    std::error_code code;
    if (std::filesystem::is_directory(location, code))
    {
        refuse(file, "is a directory");
    }
    std::ifstream input(location, std::ios::binary);
    if (!input.is_open())
    {
        refuse(file, "cannot be opened");
    }
    switch (format)
    {
    case FileFormat::Mps:
        break;
    }
    return read_mps(env, input, file);
}

void write_file(Env &env,
                const Model &model,
                const Settings &settings,
                std::string_view path,
                FileFormat format,
                const BIYOS_model *handle,
                Hooks *hooks)
{
    const EntryTimer timer(env, "io", "write", handle, hooks);
    const std::string file(path);
    switch (format)
    {
    case FileFormat::Mps:
        break;
    }
    const Draft draft = draft_of(model);
    print_file(draft, file, path);
    warn_leftovers(env, model, settings, file);
}

}
