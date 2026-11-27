#include "model/model.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/utils/checks.hpp"
#include "model/utils/renumber.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

void check_use(Index function, std::string_view user, std::size_t index, std::span<const Index> map)
{
    if (map[to_size(function)] == kDeleted)
    {
        const std::string reason = "used by " + item(user, to_index(index));
        throw Error(ErrorKind::InUse, item("function", function), reason);
    }
}

template <typename Record>
void check_records(std::span<const Record> records, Family family, std::span<const Index> map)
{
    for (std::size_t k = 0; k < records.size(); ++k)
    {
        check_use(records[k].function, noun_of(family), k, map);
    }
}

void check_cones(std::span<const Offset> ends,
                 std::span<const Index> functions,
                 std::span<const Index> map)
{
    std::size_t begin = 0;
    for (std::size_t cone = 0; cone < ends.size(); ++cone)
    {
        const auto end = to_size(ends[cone]);
        for (const Index function : functions.subspan(begin, end - begin))
        {
            check_use(function, noun_of(Family::Cone), cone, map);
        }
        begin = end;
    }
}

void check_complements(std::span<const Complement> complements, std::span<const Index> map)
{
    for (std::size_t k = 0; k < complements.size(); ++k)
    {
        check_use(complements[k].function, noun_of(Family::Complement), k, map);
        check_use(complements[k].multiplier, noun_of(Family::Complement), k, map);
    }
}

void check_callbacks(std::span<const Callback> callbacks, std::span<const Index> map)
{
    for (std::size_t k = 0; k < callbacks.size(); ++k)
    {
        for (const Index function : callbacks[k].covered)
        {
            check_use(function, "callback", k, map);
        }
    }
}

template <typename Record>
void renumber_records(std::span<Record> records, std::span<const Index> map) noexcept
{
    for (Record &record : records)
    {
        renumber(record.function, map);
    }
}

}

void Model::check_unused(std::span<const Index> map) const
{
    check_records<Constraint>(constraints_, Family::Constraint, map);
    check_records<Objective>(objectives_, Family::Objective, map);
    check_records<Indicator>(indicators_, Family::Indicator, map);
    check_cones(cone_ends_, cone_functions_, map);
    check_complements(complements_, map);
    check_callbacks(callbacks_, map);
}

void Model::renumber_functions(std::span<const Index> map) noexcept
{
    renumber_records<Constraint>(constraints_, map);
    renumber_records<Objective>(objectives_, map);
    renumber_records<Indicator>(indicators_, map);
    renumber_all(cone_functions_, map);
    for (Complement &complement : complements_)
    {
        renumber(complement.function, map);
        renumber(complement.multiplier, map);
    }
    for (Callback &callback : callbacks_)
    {
        renumber_all(callback.covered, map);
    }
}

}
