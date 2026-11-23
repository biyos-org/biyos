#include "io/files.hpp"

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/timer.hpp"
#include "io/mps.hpp"
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

}

Format find_format(int id)
{
    for (const Format format : kFormats)
    {
        if (static_cast<int>(format) == id)
        {
            return format;
        }
    }
    throw Error(ErrorKind::InvalidArgument, "format " + std::to_string(id), "unknown");
}

Model read_file(Env &env, std::string_view path, Format format)
{
    TimingTree tree;
    const Timer timer(env, tree, "io", "read");
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
    case Format::Mps:
        break;
    }
    return read_mps(env, input, file);
}

}
