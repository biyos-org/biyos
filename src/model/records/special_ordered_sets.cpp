#include "model/model.hpp"

#include "core/error.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/renumber.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "set";

void check_kind(Index sos, SpecialOrderedSetKind kind)
{
    if (std::ranges::find(kSpecialOrderedSetKinds, kind) == kSpecialOrderedSetKinds.end())
    {
        refuse(kNoun, sos, "kind " + text_of(static_cast<int>(kind)) + " is unknown");
    }
}

void check_member(Index sos, const Member &member, Index variables)
{
    const std::string variable = item("variable", member.variable);
    if (member.variable < 0 || member.variable >= variables)
    {
        refuse(kNoun, sos, variable + " is unknown");
    }
    const std::string_view fault = value_fault(member.weight);
    if (!fault.empty())
    {
        refuse(kNoun, sos, "weight of " + variable + " " + std::string(fault));
    }
}

bool not_below(const Member &a, const Member &b) noexcept
{
    return !(a.weight < b.weight);
}

void check_repeats(Index sos, std::span<const Member> members)
{
    std::vector<Member> sorted(members.begin(), members.end());
    std::ranges::sort(sorted, {}, &Member::variable);
    const auto variable = std::ranges::adjacent_find(sorted, {}, &Member::variable);
    if (variable != sorted.end())
    {
        refuse(kNoun, sos, item("variable", variable->variable) + " is repeated");
    }
    std::ranges::stable_sort(sorted, {}, &Member::weight);
    const auto weight = std::ranges::adjacent_find(sorted, not_below);
    if (weight != sorted.end())
    {
        refuse(kNoun, sos, "weight " + text_of(weight->weight) + " is repeated");
    }
}

void check_special_ordered_set(Index sos,
                               SpecialOrderedSetKind kind,
                               std::span<const Member> members,
                               Index variables)
{
    check_kind(sos, kind);
    for (const Member &member : members)
    {
        check_member(sos, member, variables);
    }
    check_repeats(sos, members);
}

}

Index Model::n_special_ordered_sets() const noexcept
{
    return to_index(special_ordered_set_kinds_.size());
}

SpecialOrderedSetKind Model::kind_of_special_ordered_set(Index index) const
{
    check_index(kNoun, index, n_special_ordered_sets());
    return special_ordered_set_kinds_[to_size(index)];
}

std::span<const Member> Model::members(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_special_ordered_sets());
    const auto begin = to_size(member_begin(to_size(start)));
    const auto end = to_size(member_begin(to_size(stop)));
    return std::span(members_).subspan(begin, end - begin);
}

void Model::check_special_ordered_set_room(std::size_t soss, Offset members) const
{
    check_n(kNoun, special_ordered_set_kinds_, soss);
    if (members > to_offset(room(members_)))
    {
        throw Error(ErrorKind::NoMemory, "model", "no member offset left");
    }
}

void Model::add_special_ordered_sets(std::span<const SpecialOrderedSetKind> kinds,
                                     std::span<const Offset> starts,
                                     std::span<const Member> members)
{
    check_special_ordered_set_room(kinds.size(), to_offset(members.size()));
    check_shape(kinds.size(), starts, members.size());
    for (std::size_t k = 0; k < kinds.size(); ++k)
    {
        const auto sos = to_index(special_ordered_set_kinds_.size() + k);
        check_special_ordered_set(sos, kinds[k], slice(members, starts, k), n_variables());
    }
    reserve_more(special_ordered_set_kinds_, kinds.size());
    reserve_more(special_ordered_set_ends_, kinds.size());
    reserve_more(members_, members.size());
    const auto base = to_offset(members_.size());
    special_ordered_set_kinds_.insert(special_ordered_set_kinds_.end(), kinds.begin(), kinds.end());
    for (const Offset end : starts.subspan(1))
    {
        special_ordered_set_ends_.push_back(base + end);
    }
    members_.insert(members_.end(), members.begin(), members.end());
}

Offset Model::member_begin(std::size_t sos) const noexcept
{
    return sos == 0 ? 0 : special_ordered_set_ends_[sos - 1];
}

std::vector<Index> Model::delete_special_ordered_sets(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_special_ordered_sets(), indices);
    keep(special_ordered_set_kinds_, map);
    keep_parts(special_ordered_set_ends_, members_, map);
    renumber_names(Family::SpecialOrderedSet, map);
    return map;
}

}
