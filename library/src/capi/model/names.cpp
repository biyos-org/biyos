#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using biyos::Error;
using biyos::ErrorKind;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Session;
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
        throw Error(ErrorKind::InvalidArgument, "argument 'name'", reason);
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

void set_model_name(Session &session, const char *name)
{
    writable(session).set_name(require(name, "name"));
}

void get_model_name(const Session &session, const char **name)
{
    const char *found = text(session.model().name());
    *require(name, "name") = found;
}

void delete_model_name(Session &session)
{
    writable(session).delete_name();
}

template <Family Target, typename Selection>
void set_names(Session &session, Selection selection, const char *const *names)
{
    Model &model = writable(session);
    const auto changed = selected(Target, model.n_of(Target), selection);
    model.set_names(Target, changed, texts(changed.size(), names));
}

template <Family Target, typename Selection>
void get_names(const Session &session, Selection selection, const char **names)
{
    const auto wanted = selected(Target, session.model().n_of(Target), selection);
    std::vector<const char *> found;
    found.reserve(wanted.size());
    for (const Index i : wanted)
    {
        found.push_back(text(session.model().name_of(Target, i)));
    }
    if (names != nullptr)
    {
        std::ranges::copy(found, names);
    }
}

template <Family Target>
void get_indices(const Session &session,
                 BIYOS_index n,
                 const char *const *names,
                 BIYOS_index *indices)
{
    const std::vector<std::string_view> wanted = texts(length(n), names);
    std::vector<Index> found;
    found.reserve(wanted.size());
    for (const std::string_view name : wanted)
    {
        found.push_back(session.model().index_of(Target, name));
    }
    if (indices != nullptr)
    {
        std::ranges::copy(found, indices);
    }
}

template <Family Target, typename Selection>
void delete_names(Session &session, Selection selection)
{
    Model &model = writable(session);
    model.delete_names(Target, selected(Target, model.n_of(Target), selection));
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
                                const BIYOS_index *var,
                                const char *const *name)
{
    return guarded<set_names<Family::Variable, List>>(mdl, List{n, var}, name);
}

BIYOS_status BIYOS_setvarnamerg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char *const *name)
{
    return guarded<set_names<Family::Variable, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getvarnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *var,
                                const char **name)
{
    return guarded<get_names<Family::Variable, List>>(mdl, List{n, var}, name);
}

BIYOS_status BIYOS_getvarnamerg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char **name)
{
    return guarded<get_names<Family::Variable, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getvaridx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *var)
{
    return guarded<get_indices<Family::Variable>>(mdl, n, name, var);
}

BIYOS_status BIYOS_delvarnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *var)
{
    return guarded<delete_names<Family::Variable, List>>(mdl, List{n, var});
}

BIYOS_status BIYOS_delvarnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::Variable, Range>>(mdl, Range{beg, end});
}

BIYOS_status BIYOS_setfunnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *fun,
                                const char *const *name)
{
    return guarded<set_names<Family::Function, List>>(mdl, List{n, fun}, name);
}

BIYOS_status BIYOS_setfunnamerg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char *const *name)
{
    return guarded<set_names<Family::Function, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getfunnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *fun,
                                const char **name)
{
    return guarded<get_names<Family::Function, List>>(mdl, List{n, fun}, name);
}

BIYOS_status BIYOS_getfunnamerg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char **name)
{
    return guarded<get_names<Family::Function, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getfunidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *fun)
{
    return guarded<get_indices<Family::Function>>(mdl, n, name, fun);
}

BIYOS_status BIYOS_delfunnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *fun)
{
    return guarded<delete_names<Family::Function, List>>(mdl, List{n, fun});
}

BIYOS_status BIYOS_delfunnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::Function, Range>>(mdl, Range{beg, end});
}

BIYOS_status BIYOS_setconnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *con,
                                const char *const *name)
{
    return guarded<set_names<Family::Constraint, List>>(mdl, List{n, con}, name);
}

BIYOS_status BIYOS_setconnamerg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char *const *name)
{
    return guarded<set_names<Family::Constraint, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getconnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *con,
                                const char **name)
{
    return guarded<get_names<Family::Constraint, List>>(mdl, List{n, con}, name);
}

BIYOS_status BIYOS_getconnamerg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char **name)
{
    return guarded<get_names<Family::Constraint, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getconidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *con)
{
    return guarded<get_indices<Family::Constraint>>(mdl, n, name, con);
}

BIYOS_status BIYOS_delconnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *con)
{
    return guarded<delete_names<Family::Constraint, List>>(mdl, List{n, con});
}

BIYOS_status BIYOS_delconnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::Constraint, Range>>(mdl, Range{beg, end});
}

BIYOS_status BIYOS_setobjnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *obj,
                                const char *const *name)
{
    return guarded<set_names<Family::Objective, List>>(mdl, List{n, obj}, name);
}

BIYOS_status BIYOS_setobjnamerg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char *const *name)
{
    return guarded<set_names<Family::Objective, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getobjnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *obj,
                                const char **name)
{
    return guarded<get_names<Family::Objective, List>>(mdl, List{n, obj}, name);
}

BIYOS_status BIYOS_getobjnamerg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char **name)
{
    return guarded<get_names<Family::Objective, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getobjidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *obj)
{
    return guarded<get_indices<Family::Objective>>(mdl, n, name, obj);
}

BIYOS_status BIYOS_delobjnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *obj)
{
    return guarded<delete_names<Family::Objective, List>>(mdl, List{n, obj});
}

BIYOS_status BIYOS_delobjnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::Objective, Range>>(mdl, Range{beg, end});
}

BIYOS_status BIYOS_setindnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *ind,
                                const char *const *name)
{
    return guarded<set_names<Family::Indicator, List>>(mdl, List{n, ind}, name);
}

BIYOS_status BIYOS_setindnamerg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char *const *name)
{
    return guarded<set_names<Family::Indicator, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getindnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *ind,
                                const char **name)
{
    return guarded<get_names<Family::Indicator, List>>(mdl, List{n, ind}, name);
}

BIYOS_status BIYOS_getindnamerg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char **name)
{
    return guarded<get_names<Family::Indicator, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getindidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *ind)
{
    return guarded<get_indices<Family::Indicator>>(mdl, n, name, ind);
}

BIYOS_status BIYOS_delindnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *ind)
{
    return guarded<delete_names<Family::Indicator, List>>(mdl, List{n, ind});
}

BIYOS_status BIYOS_delindnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::Indicator, Range>>(mdl, Range{beg, end});
}

BIYOS_status BIYOS_setsosnamesp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *sos,
                                const char *const *name)
{
    return guarded<set_names<Family::SpecialOrderedSet, List>>(mdl, List{n, sos}, name);
}

BIYOS_status BIYOS_setsosnamerg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char *const *name)
{
    return guarded<set_names<Family::SpecialOrderedSet, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getsosnamesp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *sos,
                                const char **name)
{
    return guarded<get_names<Family::SpecialOrderedSet, List>>(mdl, List{n, sos}, name);
}

BIYOS_status BIYOS_getsosnamerg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const char **name)
{
    return guarded<get_names<Family::SpecialOrderedSet, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getsosidx(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const char *const *name,
                             BIYOS_index *sos)
{
    return guarded<get_indices<Family::SpecialOrderedSet>>(mdl, n, name, sos);
}

BIYOS_status BIYOS_delsosnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *sos)
{
    return guarded<delete_names<Family::SpecialOrderedSet, List>>(mdl, List{n, sos});
}

BIYOS_status BIYOS_delsosnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::SpecialOrderedSet, Range>>(mdl, Range{beg, end});
}

BIYOS_status BIYOS_setconenamesp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *cone,
                                 const char *const *name)
{
    return guarded<set_names<Family::Cone, List>>(mdl, List{n, cone}, name);
}

BIYOS_status BIYOS_setconenamerg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const char *const *name)
{
    return guarded<set_names<Family::Cone, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getconenamesp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *cone,
                                 const char **name)
{
    return guarded<get_names<Family::Cone, List>>(mdl, List{n, cone}, name);
}

BIYOS_status BIYOS_getconenamerg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const char **name)
{
    return guarded<get_names<Family::Cone, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getconeidx(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const char *const *name,
                              BIYOS_index *cone)
{
    return guarded<get_indices<Family::Cone>>(mdl, n, name, cone);
}

BIYOS_status BIYOS_delconenamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *cone)
{
    return guarded<delete_names<Family::Cone, List>>(mdl, List{n, cone});
}

BIYOS_status BIYOS_delconenamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::Cone, Range>>(mdl, Range{beg, end});
}

BIYOS_status BIYOS_setcconnamesp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *ccon,
                                 const char *const *name)
{
    return guarded<set_names<Family::Complement, List>>(mdl, List{n, ccon}, name);
}

BIYOS_status BIYOS_setcconnamerg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const char *const *name)
{
    return guarded<set_names<Family::Complement, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getcconnamesp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *ccon,
                                 const char **name)
{
    return guarded<get_names<Family::Complement, List>>(mdl, List{n, ccon}, name);
}

BIYOS_status BIYOS_getcconnamerg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const char **name)
{
    return guarded<get_names<Family::Complement, Range>>(mdl, Range{beg, end}, name);
}

BIYOS_status BIYOS_getcconidx(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const char *const *name,
                              BIYOS_index *ccon)
{
    return guarded<get_indices<Family::Complement>>(mdl, n, name, ccon);
}

BIYOS_status BIYOS_delcconnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *ccon)
{
    return guarded<delete_names<Family::Complement, List>>(mdl, List{n, ccon});
}

BIYOS_status BIYOS_delcconnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_names<Family::Complement, Range>>(mdl, Range{beg, end});
}
