#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Family;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

const char *text(const std::string *name) noexcept
{
    return name == nullptr ? "" : name->c_str();
}

const char *entry(const char *const *names, std::size_t k)
{
    if (names[k] == nullptr)
    {
        const std::string reason = "entry " + std::to_string(k) + " is null";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "argument 'name'", reason);
    }
    return names[k];
}

std::vector<std::string_view> texts(std::size_t n, const char *const *names)
{
    if (n > 0)
    {
        require(names, "name");
    }
    std::vector<std::string_view> found;
    found.reserve(n);
    for (std::size_t k = 0; k < n; ++k)
    {
        found.emplace_back(entry(names, k));
    }
    return found;
}

void set_mdl_name(BIYOS_model &mdl, const char *name)
{
    writable(mdl).set_name(require(name, "name"));
}

void get_mdl_name(const BIYOS_model &mdl, const char **name)
{
    const char *found = text(mdl.core.name());
    *require(name, "name") = found;
}

void del_mdl_name(BIYOS_model &mdl)
{
    writable(mdl).del_name();
}

template <Family Target>
void set_names(BIYOS_model &mdl, int n, const int *idx, const char *const *names)
{
    biyos::Model &core = writable(mdl);
    const std::span<const biyos::Index> changed = listed(n, idx);
    core.set_names(Target, changed, texts(changed.size(), names));
}

template <Family Target>
void get_names(const BIYOS_model &mdl, int n, const int *idx, const char **names)
{
    const std::span<const biyos::Index> wanted = listed(n, idx);
    std::vector<const char *> found;
    found.reserve(wanted.size());
    for (const biyos::Index i : wanted)
    {
        found.push_back(text(mdl.core.name_of(Target, i)));
    }
    if (names != nullptr)
    {
        std::ranges::copy(found, names);
    }
}

template <Family Target>
void get_idx(const BIYOS_model &mdl, int n, const char *const *names, int *idx)
{
    const std::vector<std::string_view> wanted = texts(length(n), names);
    std::vector<biyos::Index> found;
    found.reserve(wanted.size());
    for (const std::string_view name : wanted)
    {
        found.push_back(mdl.core.idx_of(Target, name));
    }
    if (idx != nullptr)
    {
        std::ranges::copy(found, idx);
    }
}

template <Family Target> void del_names(BIYOS_model &mdl, int n, const int *idx)
{
    biyos::Model &core = writable(mdl);
    core.del_names(Target, listed(n, idx));
}

}

BIYOS_status BIYOS_setmdlname(BIYOS_model *mdl, const char *name)
{
    return guarded<set_mdl_name>(mdl, name);
}

BIYOS_status BIYOS_getmdlname(const BIYOS_model *mdl, const char **name)
{
    return guarded<get_mdl_name>(mdl, name);
}

BIYOS_status BIYOS_delmdlname(BIYOS_model *mdl)
{
    return guarded<del_mdl_name>(mdl);
}

BIYOS_status BIYOS_setvarname(BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char *const *name)
{
    return guarded<set_names<Family::Variable>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getvarname(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char **name)
{
    return guarded<get_names<Family::Variable>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getvaridx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_idx<Family::Variable>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delvarname(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<del_names<Family::Variable>>(mdl, n, idx);
}

BIYOS_status BIYOS_setfunname(BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char *const *name)
{
    return guarded<set_names<Family::Function>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getfunname(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char **name)
{
    return guarded<get_names<Family::Function>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getfunidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_idx<Family::Function>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delfunname(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<del_names<Family::Function>>(mdl, n, idx);
}

BIYOS_status BIYOS_setconname(BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char *const *name)
{
    return guarded<set_names<Family::Constraint>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getconname(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char **name)
{
    return guarded<get_names<Family::Constraint>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getconidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_idx<Family::Constraint>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delconname(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<del_names<Family::Constraint>>(mdl, n, idx);
}

BIYOS_status BIYOS_setobjname(BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char *const *name)
{
    return guarded<set_names<Family::Objective>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getobjname(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              const char **name)
{
    return guarded<get_names<Family::Objective>>(mdl, n, idx, name);
}

BIYOS_status BIYOS_getobjidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_idx<Family::Objective>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delobjname(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<del_names<Family::Objective>>(mdl, n, idx);
}
