#include "model/model.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/attributes/names.hpp"
#include "model/utils/checks.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

std::size_t slot(Family family) noexcept
{
    return static_cast<std::size_t>(family);
}

[[noreturn]] void unresolved(Family family, std::string_view name, std::int64_t n)
{
    const std::string noun(noun_of(family));
    const std::string object = noun + " '" + std::string(name) + "'";
    const std::string held = "names " + text_of(n) + " " + noun + "s";
    throw Error(ErrorKind::InvalidArgument, object, n == 0 ? "unknown" : held);
}

}

std::string_view noun_of(Family family) noexcept
{
    switch (family)
    {
    case Family::Variable:
        return "variable";
    case Family::Function:
        return "function";
    case Family::Constraint:
        return "constraint";
    case Family::Objective:
        return "objective";
    case Family::Indicator:
        return "indicator";
    case Family::SpecialOrderedSet:
        return "set";
    case Family::Cone:
        return "cone";
    case Family::Complement:
        return "complement";
    }
    return "record";
}

const std::string *Model::name() const noexcept
{
    return name_.empty() ? nullptr : &name_;
}

void Model::set_name(std::string_view name)
{
    const std::string fault = name_fault(name);
    if (!fault.empty())
    {
        throw Error(ErrorKind::InvalidArgument, "model", fault);
    }
    name_.assign(name);
}

void Model::delete_name() noexcept
{
    name_.clear();
}

const std::string *Model::name_of(Family family, Index index) const
{
    check_index(noun_of(family), index, n_of(family));
    return names_[slot(family)].name(index);
}

Index Model::index_of(Family family, std::string_view name) const
{
    const Holders found = names_[slot(family)].find(name);
    if (found.count != 1)
    {
        unresolved(family, name, found.count);
    }
    return found.index;
}

void Model::set_names(Family family,
                      std::span<const Index> indices,
                      std::span<const std::string_view> names)
{
    names_[slot(family)].set(noun_of(family), n_of(family), indices, names);
}

void Model::delete_names(Family family, std::span<const Index> indices)
{
    names_[slot(family)].remove(noun_of(family), n_of(family), indices);
}

void Model::renumber_names(Family family, std::span<const Index> map) noexcept
{
    names_[slot(family)].renumber(map);
}

Index Model::n_of(Family family) const noexcept
{
    switch (family)
    {
    case Family::Variable:
        return n_variables();
    case Family::Function:
        return n_functions();
    case Family::Constraint:
        return n_constraints();
    case Family::Objective:
        return n_objectives();
    case Family::Indicator:
        return n_indicators();
    case Family::SpecialOrderedSet:
        return n_special_ordered_sets();
    case Family::Cone:
        return n_cones();
    case Family::Complement:
        return n_complements();
    }
    return 0;
}

}
