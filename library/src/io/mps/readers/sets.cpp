#include "io/mps/readers/sets.hpp"

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/records.hpp"
#include "io/report.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

struct SetCode
{
    std::string_view code;
    SpecialOrderedSetKind kind;
};

constexpr std::array kSetCodes{
    SetCode{"S1", SpecialOrderedSetKind::One},
    SetCode{"S2", SpecialOrderedSetKind::Two},
};

}

std::optional<SpecialOrderedSetKind> set_kind(std::string_view code) noexcept
{
    const auto found = std::ranges::find(kSetCodes, code, &SetCode::code);
    if (found == kSetCodes.end())
    {
        return std::nullopt;
    }
    return found->kind;
}

std::string_view code_of(SpecialOrderedSetKind kind) noexcept
{
    const auto found = std::ranges::find(kSetCodes, kind, &SetCode::kind);
    return found == kSetCodes.end() ? std::string_view() : found->code;
}

void Sets::read(const Lines &lines, Draft &draft)
{
    const std::optional<SpecialOrderedSetKind> kind = set_kind(lines.fields().front());
    if (kind)
    {
        open(lines, draft, *kind);
    }
    else
    {
        add(lines, draft);
    }
}

void Sets::open(const Lines &lines, Draft &draft, SpecialOrderedSetKind kind)
{
    const Fields fields = uncommented(lines.fields(), 2);
    if (fields.size() > 2)
    {
        bad_file(lines, "a set line holds a type and a name");
    }
    DraftSet set{.kind = kind, .name = std::nullopt, .members = {}};
    if (fields.size() == 2)
    {
        check_name(lines, fields[1]);
        set.name = std::string(fields[1]);
    }
    draft.sets.push_back(std::move(set));
    columns_.clear();
    weights_.clear();
}

void Sets::add(const Lines &lines, Draft &draft)
{
    const Fields fields = uncommented(lines.fields(), 1);
    if (fields.size() > 2)
    {
        bad_file(lines, "a set member holds a column and a weight");
    }
    if (draft.sets.empty())
    {
        bad_file(lines, "a set member comes before its set");
    }
    std::vector<Member> &members = draft.sets.back().members;
    const Index column = find_in(draft.column_names, lines, "column", fields[0]);
    const Real position = static_cast<Real>(members.size() + 1);
    const Real weight = fields.size() == 2 ? value_of(lines, fields[1]) : position;
    const std::string name = quote(fields[0]);
    if (!columns_.insert(column).second)
    {
        bad_file(lines, "column " + name + " is repeated in the set");
    }
    if (!weights_.insert(weight).second)
    {
        bad_file(lines, "the weight of column " + name + " is repeated in the set");
    }
    members.push_back(Member{.variable = column, .weight = weight});
}

}
