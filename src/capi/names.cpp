#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/selection.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using biyos::Family;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
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

void set_model_name(BIYOS_model &model, const char *name)
{
    writable(model).set_name(require(name, "name"));
}

void get_model_name(const BIYOS_model &model, const char **name)
{
    const char *found = text(model.core.name());
    *require(name, "name") = found;
}

void delete_model_name(BIYOS_model &model)
{
    writable(model).delete_name();
}

template <Family Target, typename Selection>
void set_names(BIYOS_model &model, Selection selection, const char *const *names)
{
    biyos::Model &core = writable(model);
    const auto changed = selected(core, Target, selection);
    core.set_names(Target, changed, texts(changed.size(), names));
}

template <Family Target, typename Selection>
void get_names(const BIYOS_model &model, Selection selection, const char **names)
{
    const auto wanted = selected(model.core, Target, selection);
    std::vector<const char *> found;
    found.reserve(wanted.size());
    for (const biyos::Index i : wanted)
    {
        found.push_back(text(model.core.name_of(Target, i)));
    }
    if (names != nullptr)
    {
        std::ranges::copy(found, names);
    }
}

template <Family Target>
void get_indices(const BIYOS_model &model, int n, const char *const *names, int *indices)
{
    const std::vector<std::string_view> wanted = texts(length(n), names);
    std::vector<biyos::Index> found;
    found.reserve(wanted.size());
    for (const std::string_view name : wanted)
    {
        found.push_back(model.core.index_of(Target, name));
    }
    if (indices != nullptr)
    {
        std::ranges::copy(found, indices);
    }
}

template <Family Target, typename Selection>
void delete_names(BIYOS_model &model, Selection selection)
{
    biyos::Model &core = writable(model);
    core.delete_names(Target, selected(core, Target, selection));
}

}

BIYOS_status BIYOS_setmdlname(BIYOS_model *mdl, const char *name)
{
    return guarded<set_model_name>(mdl, name);
}

BIYOS_status BIYOS_getmdlname(const BIYOS_model *mdl, const char **name)
{
    return guarded<get_model_name>(mdl, name);
}

BIYOS_status BIYOS_delmdlname(BIYOS_model *mdl)
{
    return guarded<delete_model_name>(mdl);
}

BIYOS_status BIYOS_setvarnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char *const *name)
{
    return guarded<set_names<Family::Variable, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setvarnamerg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char *const *name)
{
    return guarded<set_names<Family::Variable, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getvarnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char **name)
{
    return guarded<get_names<Family::Variable, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getvarnamerg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char **name)
{
    return guarded<get_names<Family::Variable, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getvaridx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_indices<Family::Variable>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delvarnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Variable, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delvarnamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Variable, Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_setfunnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char *const *name)
{
    return guarded<set_names<Family::Function, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setfunnamerg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char *const *name)
{
    return guarded<set_names<Family::Function, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getfunnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char **name)
{
    return guarded<get_names<Family::Function, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getfunnamerg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char **name)
{
    return guarded<get_names<Family::Function, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getfunidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_indices<Family::Function>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delfunnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Function, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delfunnamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Function, Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_setconnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char *const *name)
{
    return guarded<set_names<Family::Constraint, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setconnamerg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char *const *name)
{
    return guarded<set_names<Family::Constraint, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getconnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char **name)
{
    return guarded<get_names<Family::Constraint, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getconnamerg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char **name)
{
    return guarded<get_names<Family::Constraint, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getconidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_indices<Family::Constraint>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delconnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Constraint, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delconnamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Constraint, Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_setobjnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char *const *name)
{
    return guarded<set_names<Family::Objective, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setobjnamerg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char *const *name)
{
    return guarded<set_names<Family::Objective, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getobjnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char **name)
{
    return guarded<get_names<Family::Objective, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getobjnamerg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char **name)
{
    return guarded<get_names<Family::Objective, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getobjidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_indices<Family::Objective>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delobjnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Objective, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delobjnamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Objective, Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_setindnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char *const *name)
{
    return guarded<set_names<Family::Indicator, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setindnamerg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char *const *name)
{
    return guarded<set_names<Family::Indicator, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getindnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char **name)
{
    return guarded<get_names<Family::Indicator, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getindnamerg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char **name)
{
    return guarded<get_names<Family::Indicator, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getindidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_indices<Family::Indicator>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delindnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Indicator, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delindnamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Indicator, Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_setsosnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char *const *name)
{
    return guarded<set_names<Family::Sos, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setsosnamerg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char *const *name)
{
    return guarded<set_names<Family::Sos, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getsosnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const char **name)
{
    return guarded<get_names<Family::Sos, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getsosnamerg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const char **name)
{
    return guarded<get_names<Family::Sos, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getsosidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *idx)
{
    return guarded<get_indices<Family::Sos>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delsosnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Sos, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delsosnamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Sos, Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_setconenamesp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const char *const *name)
{
    return guarded<set_names<Family::Cone, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setconenamerg(BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 const char *const *name)
{
    return guarded<set_names<Family::Cone, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getconenamesp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const char **name)
{
    return guarded<get_names<Family::Cone, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getconenamerg(const BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 const char **name)
{
    return guarded<get_names<Family::Cone, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getconeidx(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const char *const *name,
                              BIYOS_index *idx)
{
    return guarded<get_indices<Family::Cone>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delconenamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Cone, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delconenamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Cone, Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_setcconnamesp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const char *const *name)
{
    return guarded<set_names<Family::Complement, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_setcconnamerg(BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 const char *const *name)
{
    return guarded<set_names<Family::Complement, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getcconnamesp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const char **name)
{
    return guarded<get_names<Family::Complement, List>>(mdl, List{n, idx}, name);
}

BIYOS_status BIYOS_getcconnamerg(const BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 const char **name)
{
    return guarded<get_names<Family::Complement, Range>>(mdl, Range{pos, n}, name);
}

BIYOS_status BIYOS_getcconidx(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const char *const *name,
                              BIYOS_index *idx)
{
    return guarded<get_indices<Family::Complement>>(mdl, n, name, idx);
}

BIYOS_status BIYOS_delcconnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_names<Family::Complement, List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_delcconnamerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<delete_names<Family::Complement, Range>>(mdl, Range{pos, n});
}
