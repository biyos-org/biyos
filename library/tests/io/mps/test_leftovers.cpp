#include "writable.hpp"

#include "core/env.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "io/files.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Env;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Family;
using biyos::FileFormat;
using biyos::Index;
using biyos::Model;
using biyos::Objective;
using biyos::Option;
using biyos::Real;
using biyos::Settings;
using biyos::write_file;
using biyos::test::built;
using biyos::test::Parts;
using biyos::test::sample;
using biyos::test::writable;

namespace
{

Model plain()
{
    return built(writable());
}

std::vector<std::string> heard(const Model &model, const Settings &settings)
{
    Env env;
    std::vector<std::string> texts;
    const auto record = [&texts](const Event &event)
    { texts.push_back(event.module + " " + event.text); };
    env.events().add_hook(EventKind::Message, EventLevel::Warning, record);
    const std::string path = testing::TempDir() + "biyos_left.mps";
    write_file(env.stream(), model, settings, path, FileFormat::Mps);
    return texts;
}

std::string left(std::string_view part)
{
    return "io file '" + testing::TempDir() + "biyos_left.mps': free MPS leaves out " +
           std::string(part);
}

}

TEST(MpsTest, Complete)
{
    EXPECT_TRUE(heard(plain(), Settings()).empty());
}

TEST(MpsTest, LeftOut)
{
    Model model = plain();
    const Index variable = 1;
    const Real absolute = 1e-3;
    const Real relative = 0.0;
    model.set_tolerances(Family::Variable,
                         std::span(&variable, 1),
                         std::span(&absolute, 1),
                         std::span(&relative, 1));
    Objective objective = model.objective(0);
    objective.relative = 1e-6;
    objective.priority = 4;
    const Index first = 0;
    model.set_objectives(std::span(&first, 1), std::span(&objective, 1));
    Env env;
    Settings settings;
    settings.set_int(Option::Seed, 3, env.stream());
    EXPECT_EQ(heard(model, settings),
              (std::vector{
                  left("the record tolerances"),
                  left("the objective tolerances"),
                  left("the objective priority"),
                  left("the model options"),
              }));
}

TEST(MpsTest, ConstraintTolerance)
{
    Model model = plain();
    const Index constraint = 2;
    const Real absolute = 0.0;
    const Real relative = 0.5;
    model.set_tolerances(Family::Constraint,
                         std::span(&constraint, 1),
                         std::span(&absolute, 1),
                         std::span(&relative, 1));
    EXPECT_EQ(heard(model, Settings()), std::vector{left("the record tolerances")});
}

TEST(MpsTest, IndicatorNames)
{
    Parts parts = writable();
    parts.indicators = sample().indicators;
    EXPECT_TRUE(heard(built(parts), Settings()).empty());
    parts.indicator_names = sample().indicator_names;
    EXPECT_EQ(heard(built(parts), Settings()), std::vector{left("the indicator names")});
}
