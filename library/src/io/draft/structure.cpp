#include "io/draft/structure.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/text/text.hpp"
#include "io/codes.hpp"
#include "io/draft/draft.hpp"
#include "io/draft/labels.hpp"
#include "io/draft/row_shape.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

DraftRow indicator_row(const Model &model, Index index)
{
    const Indicator &indicator = model.indicator(index);
    const std::string record = item("indicator", index);
    const Constraint bounds{
        .function = indicator.function,
        .lower = indicator.lower,
        .upper = indicator.upper,
    };
    const DraftRow row = row_shape(record, bounds, model.constant(indicator.function));
    if (row.range || row.sense == RowSense::Free)
    {
        const std::string interval = interval_of(indicator.lower, indicator.upper);
        const std::string reason = "bounds " + interval + " cannot be an indicator row in free MPS";
        throw Error(ErrorKind::Unsupported, record, reason);
    }
    return row;
}

bool same_bounds(const Indicator &a, const Indicator &b)
{
    return identical(a.lower, b.lower) && identical(a.upper, b.upper);
}

void add_row(Draft &draft, const Model &model, Index index, const Taken &taken)
{
    draft.rows.push_back(indicator_row(model, index));
    enter(draft.row_names, fresh("I" + text_of(index), taken), Family::Indicator, index);
}

void check_member(const Draft &draft, Index sos, const Member &member)
{
    const std::string_view column = draft.column_names.names()[to_size(member.variable)];
    if (set_kind(column))
    {
        unwritable(item("set", sos), "member '" + std::string(column) + "'");
    }
}

}

void add_indicators(Draft &draft, const Model &model, std::vector<Index> &functions)
{
    const Taken taken = user_names(model, {Family::Objective, Family::Constraint});
    std::map<Index, Index> first;
    for (Index k = 0; k < model.n_indicators(); ++k)
    {
        const Indicator &indicator = model.indicator(k);
        const auto [found, added] = first.try_emplace(indicator.function, k);
        const bool shared = !added && same_bounds(model.indicator(found->second), indicator);
        if (!shared)
        {
            add_row(draft, model, k, taken);
            functions.push_back(indicator.function);
        }
        const Index last = to_index(draft.rows.size()) - 1;
        draft.indicators.push_back(DraftIndicator{
            .row = shared ? draft.indicators[to_size(found->second)].row : last,
            .column = indicator.variable,
            .value = indicator.value,
        });
    }
}

void add_sets(Draft &draft, const Model &model)
{
    for (Index k = 0; k < model.n_special_ordered_sets(); ++k)
    {
        const std::span<const Member> members = model.members(k, k + 1);
        for (const Member &member : members)
        {
            check_member(draft, k, member);
        }
        draft.sets.push_back(DraftSet{
            .kind = model.kind_of_special_ordered_set(k),
            .name = kept_label(model, Family::SpecialOrderedSet, k),
            .members = {members.begin(), members.end()},
        });
    }
}

}
