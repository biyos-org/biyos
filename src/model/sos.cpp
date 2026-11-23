#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"

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

void check_kind(Index sos, SosKind kind)
{
    if (std::ranges::find(kSosKinds, kind) == kSosKinds.end())
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

void check_sos(Index sos, SosKind kind, std::span<const Member> members, Index variables)
{
    check_kind(sos, kind);
    for (const Member &member : members)
    {
        check_member(sos, member, variables);
    }
    check_repeats(sos, members);
}

}

Index Model::n_sos() const noexcept
{
    return static_cast<Index>(sos_kinds_.size());
}

SosKind Model::kind_of_sos(Index index) const
{
    check_index(kNoun, index, n_sos());
    return sos_kinds_[static_cast<std::size_t>(index)];
}

std::span<const Member> Model::members(Index start, Index size) const
{
    check_span(kNoun, start, size, n_sos());
    const auto begin = static_cast<std::size_t>(member_begin(static_cast<std::size_t>(start)));
    const auto end = static_cast<std::size_t>(member_begin(static_cast<std::size_t>(start + size)));
    return std::span(members_).subspan(begin, end - begin);
}

void Model::check_sos_room(std::size_t sos, Offset members) const
{
    check_n(kNoun, sos_kinds_, sos);
    if (members > static_cast<Offset>(room(members_)))
    {
        throw Error(ErrorKind::NoMemory, "model", "no member offset left");
    }
}

void Model::add_sos(std::span<const SosKind> kinds,
                    std::span<const Offset> starts,
                    std::span<const Member> members)
{
    check_sos_room(kinds.size(), static_cast<Offset>(members.size()));
    check_shape(kinds.size(), starts, members.size());
    for (std::size_t k = 0; k < kinds.size(); ++k)
    {
        const auto sos = static_cast<Index>(sos_kinds_.size() + k);
        check_sos(sos, kinds[k], slice(members, starts, k), n_variables());
    }
    reserve_more(sos_kinds_, kinds.size());
    reserve_more(sos_ends_, kinds.size());
    reserve_more(members_, members.size());
    const auto base = static_cast<Offset>(members_.size());
    sos_kinds_.insert(sos_kinds_.end(), kinds.begin(), kinds.end());
    for (const Offset end : starts.subspan(1))
    {
        sos_ends_.push_back(base + end);
    }
    members_.insert(members_.end(), members.begin(), members.end());
}

Offset Model::member_begin(std::size_t sos) const noexcept
{
    return sos == 0 ? 0 : sos_ends_[sos - 1];
}

}
