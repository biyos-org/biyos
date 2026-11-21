#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "model/checks.hpp"
#include "model/names.hpp"

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

[[noreturn]] void unresolved(Family family, std::string_view name, std::int64_t count)
{
    const std::string noun(noun_of(family));
    const std::string object = noun + " '" + std::string(name) + "'";
    const std::string held = "names " + text_of(count) + " " + noun + "s";
    throw Error(ErrorKind::InvalidArgument, object, count == 0 ? "unknown" : held);
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

void Model::del_name() noexcept
{
    name_.clear();
}

const std::string *Model::name_of(Family family, Index index) const
{
    check_index(noun_of(family), index, count_of(family));
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
    names_[slot(family)].set(noun_of(family), count_of(family), indices, names);
}

void Model::del_names(Family family, std::span<const Index> indices)
{
    names_[slot(family)].remove(noun_of(family), count_of(family), indices);
}

Index Model::count_of(Family family) const noexcept
{
    switch (family)
    {
    case Family::Variable:
        return variable_count();
    case Family::Function:
        return function_count();
    case Family::Constraint:
        return constraint_count();
    case Family::Objective:
        return objective_count();
    }
    return 0;
}

}
