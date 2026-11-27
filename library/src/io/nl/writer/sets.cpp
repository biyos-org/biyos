#include "io/nl/writer/sets.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <ostream>
#include <span>
#include <vector>

namespace biyos::nl
{

namespace
{

struct Entry
{
    Index position;
    Offset number;
    Real weight;
};

Offset number_of(const Model &model, Index set)
{
    const Offset number = static_cast<Offset>(set) + 1;
    return model.kind_of_special_ordered_set(set) == SpecialOrderedSetKind::One ? number : -number;
}

std::vector<Entry> entries_of(const Model &model, std::span<const Index> position)
{
    std::vector<Entry> entries;
    for (Index set = 0; set < model.n_special_ordered_sets(); ++set)
    {
        const Offset number = number_of(model, set);
        for (const Member &member : model.members(set, set + 1))
        {
            entries.push_back({.position = position[to_size(member.variable)],
                               .number = number,
                               .weight = member.weight});
        }
    }
    std::ranges::sort(entries, {}, &Entry::position);
    return entries;
}

}

void print_sets(std::ostream &output, const Model &model, std::span<const Index> position)
{
    if (model.n_special_ordered_sets() == 0)
    {
        return;
    }
    const std::vector<Entry> entries = entries_of(model, position);
    output << "S0 " << entries.size() << " sosno\n";
    for (const Entry &entry : entries)
    {
        output << entry.position << ' ' << entry.number << '\n';
    }
    output << "S4 " << entries.size() << " ref\n";
    for (const Entry &entry : entries)
    {
        output << entry.position << ' ' << text_of(entry.weight) << '\n';
    }
}

}
