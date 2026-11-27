#include "io/native/readers/records.hpp"

#include "core/numbers.hpp"
#include "io/native/utils/body.hpp"
#include "io/native/utils/record.hpp"
#include "io/native/utils/words.hpp"
#include "model/model.hpp"

#include <vector>

namespace biyos::native
{

namespace
{

Variable variable_of(Record record)
{
    const Variable variable{
        .kind = lookup(record, kVariableWords, "a variable kind"),
        .lower = record.real(),
        .upper = record.real(),
    };
    record.end();
    return variable;
}

Constraint constraint_of(Record record)
{
    const Constraint constraint{
        .function = record.index(),
        .lower = record.real(),
        .upper = record.real(),
    };
    record.end();
    return constraint;
}

Objective objective_of(Record record)
{
    const Objective objective{
        .function = record.index(),
        .sense = lookup(record, kSenseWords, "an objective sense"),
        .priority = record.integer(),
        .weight = record.real(),
        .absolute = record.real(),
        .relative = record.real(),
    };
    record.end();
    return objective;
}

Indicator indicator_of(Record record)
{
    const Indicator indicator{
        .variable = record.index(),
        .value = record.integer(),
        .function = record.index(),
        .lower = record.real(),
        .upper = record.real(),
    };
    record.end();
    return indicator;
}

Complement complement_of(Record record)
{
    const Complement complement{
        .function = record.index(),
        .lower = record.real(),
        .upper = record.real(),
        .multiplier = record.index(),
    };
    record.end();
    return complement;
}

template <typename Item, typename Parse> std::vector<Item> items_of(Body &body, Parse parse)
{
    std::vector<Item> items;
    while (body.next())
    {
        items.push_back(parse(body.record()));
    }
    return items;
}

struct Sets
{
    std::vector<SpecialOrderedSetKind> kinds;
    std::vector<Offset> starts{0};
    std::vector<Member> members;
};

void add_set(Record record, Sets &sets)
{
    sets.kinds.push_back(lookup(record, kSetWords, "a set kind"));
    const Offset count = record.entries(2);
    for (Offset k = 0; k < count; ++k)
    {
        sets.members.push_back(Member{.variable = record.index(), .weight = record.real()});
    }
    record.end();
    sets.starts.push_back(to_offset(sets.members.size()));
}

struct Cones
{
    std::vector<ConeKind> kinds;
    std::vector<Offset> starts{0};
    std::vector<Index> functions;
    std::vector<Offset> parameter_starts{0};
    std::vector<Real> parameters;
};

void add_cone(Record record, Cones &cones)
{
    cones.kinds.push_back(lookup(record, kConeWords, "a cone kind"));
    const Offset members = record.entries(1);
    for (Offset k = 0; k < members; ++k)
    {
        cones.functions.push_back(record.index());
    }
    const Offset parameters = record.entries(1);
    for (Offset k = 0; k < parameters; ++k)
    {
        cones.parameters.push_back(record.real());
    }
    record.end();
    cones.starts.push_back(to_offset(cones.functions.size()));
    cones.parameter_starts.push_back(to_offset(cones.parameters.size()));
}

}

void read_variables(Body &body, Model &model)
{
    model.add_variables(items_of<Variable>(body, variable_of));
}

void read_constraints(Body &body, Model &model)
{
    model.add_constraints(items_of<Constraint>(body, constraint_of));
}

void read_objectives(Body &body, Model &model)
{
    model.add_objectives(items_of<Objective>(body, objective_of));
}

void read_indicators(Body &body, Model &model)
{
    model.add_indicators(items_of<Indicator>(body, indicator_of));
}

void read_sets(Body &body, Model &model)
{
    Sets sets;
    while (body.next())
    {
        add_set(body.record(), sets);
    }
    model.add_special_ordered_sets(sets.kinds, sets.starts, sets.members);
}

void read_cones(Body &body, Model &model)
{
    Cones cones;
    while (body.next())
    {
        add_cone(body.record(), cones);
    }
    model.add_cones(cones.kinds,
                    cones.starts,
                    cones.functions,
                    cones.parameter_starts,
                    cones.parameters);
}

void read_complements(Body &body, Model &model)
{
    model.add_complements(items_of<Complement>(body, complement_of));
}

}
