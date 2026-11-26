#include "biyos/biyos.h"

#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/events/stream.hpp"
#include "io/files.hpp"

#include <memory>
#include <string_view>

using biyos::Env;
using biyos::FileFormat;
using biyos::find_format;
using biyos::read_file;
using biyos::Session;
using biyos::write_file;
using biyos::capi::guarded;
using biyos::capi::idle;
using biyos::capi::require;

namespace
{

void load(Env &env, const char *path, BIYOS_int fmt, BIYOS_model **mdl)
{
    const char *file = require(path, "path");
    BIYOS_model **made = require(mdl, "mdl");
    const FileFormat format = find_format(fmt);
    idle(env, "environment");
    auto fresh = std::make_unique<BIYOS_model>(env);
    fresh->change() = read_file(env, std::string_view(file), format);
    *made = fresh.release();
}

void save(const Session &session, const char *path, BIYOS_int fmt)
{
    const char *file = require(path, "path");
    const FileFormat format = find_format(fmt);
    idle(session.env(), "environment");
    write_file(session.stream(),
               session.model(),
               session.settings(),
               std::string_view(file),
               format);
}

}

BIYOS_status BIYOS_readmdl(BIYOS_env *env, const char *path, BIYOS_int fmt, BIYOS_model **mdl)
{
    if (mdl != nullptr)
    {
        *mdl = nullptr;
    }
    return guarded<load>(env, path, fmt, mdl);
}

BIYOS_status BIYOS_writemdl(const BIYOS_model *mdl, const char *path, BIYOS_int fmt)
{
    return guarded<save>(mdl, path, fmt);
}
