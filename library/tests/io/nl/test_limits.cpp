#include "building.hpp"
#include "sample.hpp"
#include "written.hpp"

#include "core/env/env.hpp"
#include "core/events/event.hpp"
#include "core/numbers.hpp"
#include "core/options/settings.hpp"
#include "expr/graph/node.hpp"
#include "io/files.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using biyos::Env;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::FileFormat;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::Operator;
using biyos::Settings;
using biyos::Variable;
using biyos::VariableKind;
using biyos::write_file;
using biyos::test::built;
using biyos::test::Parts;
using biyos::test::sample;
using biyos::test::nl::node;
using biyos::test::nl::refusal;
using biyos::test::nl::reports_of;
using biyos::test::nl::tiny_wanted;
using biyos::test::nl::writable;

namespace
{

using Build = Offset (*)(Model &, Offset);

struct Refused
{
    Build build;
    std::string_view name;
};

Parts unnamed()
{
    Parts parts = writable();
    parts.name = "";
    parts.variable_names = {};
    parts.function_names = {};
    parts.constraint_names = {};
    parts.objective_names = {};
    return parts;
}

std::string refused(std::string_view record, std::string_view part)
{
    return "unsupported: " + std::string(record) + ": NL cannot hold " + std::string(part);
}

std::string warned(std::string_view text)
{
    return "file 't.nl': NL " + std::string(text);
}

std::string left_path()
{
    return testing::TempDir() + "biyos_left.nl";
}

std::string left(std::string_view text)
{
    return "file '" + left_path() + "': NL " + std::string(text);
}

std::vector<std::string> heard(const Model &model)
{
    Env env;
    std::vector<std::string> texts;
    const auto record = [&texts](const Event &event) { texts.push_back(event.text); };
    env.events().add_hook(EventKind::Message, EventLevel::Info, record);
    write_file(env.stream(), model, Settings(), left_path(), FileFormat::Nl);
    return texts;
}

Offset entropy_of(Model &model, Offset x)
{
    return node(model, Operator::Entropy, {x}, {});
}

Offset erf_of(Model &model, Offset x)
{
    return node(model, Operator::Erf, {x}, {});
}

Offset sign_power_of(Model &model, Offset x)
{
    return node(model, Operator::SignPower, {x}, {1.5});
}

Offset atan2_of(Model &model, Offset x)
{
    return node(model, Operator::Atan2, {x, x}, {});
}

Model with_kind(VariableKind kind)
{
    Parts parts = writable();
    parts.variables[1] = Variable{.kind = kind, .lower = 1.0, .upper = 2.0};
    return built(parts);
}

}

TEST(NlTest, Writable)
{
    EXPECT_EQ(refusal(built(writable())), "");
}

TEST(NlTest, Records)
{
    const Parts base = writable();
    Parts indicators = base;
    indicators.indicators = sample().indicators;
    EXPECT_EQ(refusal(built(indicators)), refused("indicator 0", "indicators"));
    Parts sets = base;
    sets.special_ordered_sets = sample().special_ordered_sets;
    EXPECT_EQ(refusal(built(sets)), refused("set 0", "special ordered sets"));
    Parts cones = base;
    cones.cones = sample().cones;
    EXPECT_EQ(refusal(built(cones)), refused("cone 0", "cones"));
    Parts complements = base;
    complements.complements = sample().complements;
    EXPECT_EQ(refusal(built(complements)), refused("complement 0", "complements"));
    Model callbacks = built(base);
    callbacks.add_callback(std::vector<Index>{1}, nullptr);
    EXPECT_EQ(refusal(callbacks), refused("callback 0", "callbacks"));
}

TEST(NlTest, Semi)
{
    EXPECT_EQ(refusal(with_kind(VariableKind::SemiContinuous)),
              refused("variable 1", "a semi-continuous variable"));
    EXPECT_EQ(refusal(with_kind(VariableKind::SemiInteger)),
              refused("variable 1", "a semi-integer variable"));
}

TEST(NlTest, Weight)
{
    Parts parts = writable();
    parts.objectives[1].weight = 0.5;
    EXPECT_EQ(refusal(built(parts)), refused("objective 1", "a weight other than 1, here 0.5"));
}

TEST(NlTest, Operators)
{
    const std::vector<Refused> cases{
        {.build = entropy_of, .name = "entropy"},
        {.build = erf_of, .name = "erf"},
        {.build = sign_power_of, .name = "signed power"},
        {.build = atan2_of, .name = "atan2"},
    };
    for (const Refused &refusal_case : cases)
    {
        const std::string part = "the operator " + std::string(refusal_case.name);
        EXPECT_EQ(refusal(tiny_wanted(refusal_case.build)), refused("function 1", part));
    }
}

TEST(NlTest, Unused)
{
    Model model = tiny_wanted(entropy_of);
    model.set_objectives(std::vector<Index>{0}, std::vector{biyos::default_objective(0, 0)});
    EXPECT_EQ(refusal(model), "");
    const std::vector<std::string> wanted{
        warned("leaves out function 1, which no record uses"),
    };
    EXPECT_EQ(reports_of(model), wanted);
}

TEST(NlTest, Reports)
{
    const std::string binaries = warned("writes the nonlinear binary variables as integers");
    EXPECT_EQ(reports_of(built(unnamed())), std::vector{binaries});
    const std::vector<std::string> wanted{binaries, warned("leaves out the names")};
    EXPECT_EQ(reports_of(built(writable())), wanted);
}

TEST(NlTest, Leftovers)
{
    Parts parts = unnamed();
    parts.objectives[1].absolute = 0.5;
    parts.objectives[1].priority = 5;
    const std::vector<std::string> wanted{
        left("leaves out the objective tolerances"),
        left("leaves out the objective priority"),
        left("writes the nonlinear binary variables as integers"),
    };
    EXPECT_EQ(heard(built(parts)), wanted);
}
