#pragma once

#include "files.hpp"

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/event.hpp"
#include "io/nl/nl.hpp"
#include "model/model.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::test::nl
{

inline std::string bad(int line, std::string_view message)
{
    return "bad file: file 't.nl' line " + std::to_string(line) + ": " + std::string(message);
}

inline std::string refused(int line, std::string_view message)
{
    return "unsupported: file 't.nl' line " + std::to_string(line) + ": " + std::string(message);
}

inline Model read_text(Env &env, std::string_view text)
{
    std::istringstream input{std::string(text)};
    return read_nl(env.stream(), input, "t.nl");
}

inline Model read_sample(std::string_view text)
{
    Env env;
    return read_text(env, text);
}

inline std::string fault_of(std::string_view text)
{
    Env env;
    try
    {
        read_text(env, text);
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "";
}

inline std::string tiny_with(std::string_view tail)
{
    return std::string(kTinyHeader) + std::string(tail);
}

inline std::string tiny_fault(std::string_view tail)
{
    return fault_of(tiny_with(tail));
}

inline std::vector<std::string> infos_of(std::string_view text)
{
    Env env;
    std::vector<std::string> heard;
    const auto record = [&heard](const Event &event) { heard.push_back(event.text); };
    env.events().add_hook(EventKind::Message, EventLevel::Info, record);
    read_text(env, text);
    return heard;
}

}
