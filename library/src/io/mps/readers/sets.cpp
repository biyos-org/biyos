#include "io/mps/readers/sets.hpp"

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/utils/codes.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos
{

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
