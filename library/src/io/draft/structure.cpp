#include "io/draft/structure.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/draft/labels.hpp"
#include "io/mps/readers/sets.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

void check_member(const Draft &draft, Index sos, const Member &member)
{
    const std::string_view column = draft.column_names.names()[to_size(member.variable)];
    if (set_kind(column))
    {
        const std::string reason =
            "member '" + std::string(column) + "' cannot be written in free MPS";
        throw Error(ErrorKind::Unsupported, item("set", sos), reason);
    }
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
