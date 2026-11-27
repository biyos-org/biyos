#include "io/files.hpp"

#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/events/stream.hpp"
#include "core/options/lookup.hpp"
#include "core/options/settings.hpp"
#include "core/timing/timer.hpp"
#include "io/draft/draft.hpp"
#include "io/draft/sketch.hpp"
#include "io/mps/mps.hpp"
#include "io/mps/print.hpp"
#include "io/native/native.hpp"
#include "io/native/print.hpp"
#include "io/nl/nl.hpp"
#include "io/reproducer/reproducer.hpp"
#include "io/utils/leftovers.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <ostream>
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

std::string object_of(const std::string &file)
{
    return "file '" + file + "'";
}

[[noreturn]] void refuse(const std::string &file, std::string_view reason)
{
    throw Error(ErrorKind::FileAccess, object_of(file), std::string(reason));
}

std::ifstream opened(const std::string &file, std::string_view path)
{
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
    return input;
}

template <typename Print>
void print_file(const std::string &file, std::string_view path, Print print)
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
    print(output);
    output.close();
    if (output.fail())
    {
        refuse(file, "cannot be written");
    }
}

void write_native(Stream stream,
                  const Model &model,
                  const Settings &settings,
                  const std::string &file,
                  std::string_view path)
{
    check_native(model);
    print_file(file, path, [&model](std::ostream &output) { print_native(model, output); });
    if (!settings.empty())
    {
        warn(stream, file, "the native format leaves out the model options");
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
    const EntryTimer timer(env.stream(), "io", "read");
    const std::string file(path);
    std::ifstream input = opened(file, path);
    switch (format)
    {
    case FileFormat::Mps:
        return read_mps(env.stream(), input, file, MpsForm::Free);
    case FileFormat::FixedMps:
        return read_mps(env.stream(), input, file, MpsForm::Fixed);
    case FileFormat::Nl:
        return read_nl(env.stream(), input, file);
    case FileFormat::Bys:
        break;
    }
    return read_native(input, file);
}

void write_file(Stream stream,
                const Model &model,
                const Settings &settings,
                std::string_view path,
                FileFormat format)
{
    const EntryTimer timer(stream, "io", "write");
    const std::string file(path);
    switch (format)
    {
    case FileFormat::Mps:
        break;
    case FileFormat::FixedMps:
        throw Error(ErrorKind::Unsupported,
                    object_of(file),
                    "fixed MPS files are read, never written");
    case FileFormat::Nl:
        throw Error(ErrorKind::Unsupported, object_of(file), "NL files are read, never written");
    case FileFormat::Bys:
        write_native(stream, model, settings, file, path);
        return;
    }
    const Draft draft = draft_of(model);
    print_file(file, path, [&draft](std::ostream &output) { print_mps(draft, output); });
    warn_leftovers(stream, model, settings, file);
}

Model read_reproducer(Env &env, Settings &settings, std::string_view path)
{
    const EntryTimer timer(env.stream(), "io", "read");
    const std::string file(path);
    std::ifstream input = opened(file, path);
    return read_reproducer(env, settings, input, file);
}

void write_reproducer(Stream stream,
                      const Model &model,
                      const Lookup &lookup,
                      std::string_view path)
{
    const EntryTimer timer(stream, "io", "write");
    check_native(model);
    const auto print = [&](std::ostream &output) { print_reproducer(model, lookup, output); };
    print_file(std::string(path), path, print);
}

}
