#include "loading.hpp"
#include "sample.hpp"
#include "samples.hpp"
#include "texts.hpp"

#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/numbers.hpp"
#include "io/mps/mps.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <locale>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using biyos::Complement;
using biyos::Constraint;
using biyos::default_objective;
using biyos::difference;
using biyos::Env;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::read_mps;
using biyos::Real;
using biyos::test::built;
using biyos::test::expect_round;
using biyos::test::kMpsFiles;
using biyos::test::native_model;
using biyos::test::native_text;
using biyos::test::Parts;
using biyos::test::sample;
using biyos::test::nl::kNlFiles;

namespace
{

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();

constexpr std::string_view kSmallNativeFile = R"(BIYOS 1
NAME small\20model
VARIABLES 3
cont -0 2.5
bin 0 1
int -inf 3
FUNCTIONS 3
1.5
0
0.1
LINEAR 3
0 0 2
0 2 -1
1 1 0.1
PIECEWISE 1
2 0 0 1 2
EXPRESSIONS 2
0 5
var 0
exp 1
keep 1
ref 2
sum 2 0 1 -2
1 2
ref 2
pwl 0
CONSTRAINTS 1
0 -1 1
OBJECTIVES 1
1 min 0 1 0 0
NAMES variable 2
0 x
2 a\5Cb
TOLERANCES constraint 1
0 1e-06 none
END
)";

constexpr std::string_view kInterleavedNativeFile = R"(BIYOS 1
VARIABLES 3
cont -0 2.5
bin 0 1
int -inf 3
FUNCTIONS 3
1.5
0
0.1
LINEAR 3
0 0 2
1 1 1
0 2 -1
END
)";

using Keep = void (*)(Parts &kept, const Parts &all);

struct Piece
{
    std::string_view section;
    Keep keep;
};

Parts empty()
{
    Parts parts{};
    parts.starts = {0};
    parts.quadratic.starts = {0};
    parts.special_ordered_sets.starts = {0};
    parts.cones.starts = {0};
    parts.cones.parameter_starts = {0};
    return parts;
}

Parts core()
{
    const Parts all = sample();
    Parts parts = empty();
    parts.variables = all.variables;
    parts.constants = all.constants;
    parts.starts = std::vector<Offset>(all.constants.size() + 1, 0);
    return parts;
}

void keep_none(Parts &, const Parts &) {}

void keep_linear(Parts &kept, const Parts &all)
{
    kept.starts = all.starts;
    kept.terms = all.terms;
}

void keep_quadratic(Parts &kept, const Parts &all)
{
    kept.quadratic = all.quadratic;
}

void keep_expressions(Parts &kept, const Parts &all)
{
    kept.expressions = all.expressions;
}

void keep_constraints(Parts &kept, const Parts &all)
{
    kept.constraints = all.constraints;
}

void keep_objectives(Parts &kept, const Parts &all)
{
    kept.objectives = all.objectives;
}

void keep_indicators(Parts &kept, const Parts &all)
{
    kept.indicators = all.indicators;
}

void keep_sets(Parts &kept, const Parts &all)
{
    kept.special_ordered_sets = all.special_ordered_sets;
}

void keep_cones(Parts &kept, const Parts &all)
{
    kept.cones = all.cones;
}

void keep_complements(Parts &kept, const Parts &all)
{
    kept.complements = all.complements;
}

void keep_name(Parts &kept, const Parts &all)
{
    kept.name = all.name;
}

void keep_names(Parts &kept, const Parts &all)
{
    kept.variable_names = all.variable_names;
    kept.function_names = all.function_names;
}

constexpr std::array kPieces{
    Piece{.section = "FUNCTIONS", .keep = keep_none},
    Piece{.section = "LINEAR", .keep = keep_linear},
    Piece{.section = "QUADRATIC", .keep = keep_quadratic},
    Piece{.section = "EXPRESSIONS", .keep = keep_expressions},
    Piece{.section = "CONSTRAINTS", .keep = keep_constraints},
    Piece{.section = "OBJECTIVES", .keep = keep_objectives},
    Piece{.section = "INDICATORS", .keep = keep_indicators},
    Piece{.section = "SETS", .keep = keep_sets},
    Piece{.section = "CONES", .keep = keep_cones},
    Piece{.section = "COMPLEMENTS", .keep = keep_complements},
    Piece{.section = "NAME sample", .keep = keep_name},
    Piece{.section = "NAMES function", .keep = keep_names},
};

Offset unary(Model &model, int code, Offset child)
{
    const std::array children{child};
    return model.add_node(code, children, {});
}

Offset paired(Model &model, Offset first, Offset second)
{
    const std::array children{first, second};
    const std::array values{0.0, 1.0, -2.0};
    return model.add_node(BIYOS_OP_SUM, children, values);
}

void attach(Model &model, std::vector<Index> functions, std::vector<Offset> nodes)
{
    model.attach_expressions(functions, nodes);
}

void tolerate(Model &model, Family family, Index index, Real absolute, Real relative)
{
    model.set_tolerances(family, std::span(&index, 1), std::span(&absolute, 1), {});
    model.set_tolerances(family, std::span(&index, 1), {}, std::span(&relative, 1));
}

struct Thousands : std::numpunct<char>
{
    char do_thousands_sep() const override { return ','; }

    std::string do_grouping() const override { return "\3"; }
};

Model mps_model(std::string_view file)
{
    Env env;
    std::istringstream input{std::string(file)};
    return read_mps(env.stream(), input, "t.mps", biyos::MpsForm::Free);
}

Model small()
{
    Parts parts = core();
    keep_linear(parts, sample());
    parts.constraints = {Constraint{.function = 0, .lower = -1.0, .upper = 1.0}};
    parts.objectives = {default_objective(1, 0)};
    parts.name = "small model";
    parts.variable_names = {.indices = {0, 2}, .names = {"x", "a\\b"}};
    Model model = built(parts);
    const std::array x{0.0, 1.0};
    const std::array y{0.0, 2.0};
    const Offset table = model.add_piecewise_linear(x, y);
    const Offset shared = unary(model, BIYOS_OP_EXP, model.add_variable_node(0));
    attach(model, {0, 1}, {paired(model, shared, shared), model.add_piecewise(table, shared)});
    const Index constraint = 0;
    const Real absolute = 1e-6;
    model.set_tolerances(Family::Constraint,
                         std::span(&constraint, 1),
                         std::span(&absolute, 1),
                         {});
    return model;
}

}

TEST(NativeTest, Text)
{
    EXPECT_EQ(native_text(small()), kSmallNativeFile);
    EXPECT_EQ(difference(native_model(kSmallNativeFile), small()), "");
}

TEST(NativeTest, Interleaved)
{
    Parts parts = core();
    parts.starts = {0, 2, 3, 3};
    parts.terms = {
        {.variable = 0, .coefficient = 2.0},
        {.variable = 2, .coefficient = -1.0},
        {.variable = 1, .coefficient = 1.0},
    };
    EXPECT_EQ(difference(native_model(kInterleavedNativeFile), built(parts)), "");
}

TEST(NativeTest, Empty)
{
    expect_round(Model());
    EXPECT_EQ(native_text(Model()), "BIYOS 1\nEND\n");
}

TEST(NativeTest, Sample)
{
    expect_round(built(sample()));
}

TEST(NativeTest, Variables)
{
    Parts parts = empty();
    parts.variables = sample().variables;
    expect_round(built(parts));
}

TEST(NativeTest, EachPart)
{
    for (const Piece &piece : kPieces)
    {
        SCOPED_TRACE(piece.section);
        Parts parts = core();
        piece.keep(parts, sample());
        const Model model = built(parts);
        EXPECT_NE(native_text(model).find(piece.section), std::string::npos);
        expect_round(model);
    }
}

TEST(NativeTest, Piecewise)
{
    Model model = built(core());
    const std::array x{-1.0, 0.0, 2.0};
    const std::array y{1.0, 0.0, 4.0};
    const Offset table = model.add_piecewise_linear(x, y);
    const Offset first = model.add_piecewise(table, model.add_variable_node(0));
    const Offset second = model.add_piecewise(table, model.add_variable_node(2));
    attach(model, {0, 2}, {first, second});
    EXPECT_NE(native_text(model).find("PIECEWISE 1\n3 -1 1 0 0 2 4\n"), std::string::npos);
    expect_round(model);
}

TEST(NativeTest, Tolerances)
{
    Model model = built(sample());
    tolerate(model, Family::Variable, 2, 1e-3, 0.0);
    tolerate(model, Family::Cone, 1, 0.0, 0.25);
    const Index function = 1;
    const Real relative = 0.5;
    model.set_tolerances(Family::Function, std::span(&function, 1), {}, std::span(&relative, 1));
    EXPECT_NE(native_text(model).find("TOLERANCES function 1\n1 none 0.5\n"), std::string::npos);
    expect_round(model);
}

TEST(NativeTest, SharedNodes)
{
    Model model = built(core());
    const Offset shared = unary(model, BIYOS_OP_EXP, model.add_variable_node(0));
    const Offset sum = paired(model, shared, shared);
    attach(model, {0, 1, 2}, {sum, shared, sum});
    const Model back = native_model(native_text(model));
    EXPECT_EQ(back.graph().size(), model.graph().size());
    expect_round(model);
}

TEST(NativeTest, SharedFunction)
{
    Parts parts = core();
    parts.constraints = {
        Constraint{.function = 1, .lower = 0.0, .upper = 1.0},
        Constraint{.function = 1, .lower = -kInfinity, .upper = 2.0},
    };
    parts.objectives = {default_objective(1, 0)};
    parts.complements = {
        Complement{.function = 1, .lower = 0.0, .upper = kInfinity, .multiplier = 0},
    };
    const Model model = built(parts);
    const Model back = native_model(native_text(model));
    EXPECT_EQ(back.n_functions(), 3);
    EXPECT_EQ(back.n_constraints(), 2);
    expect_round(model);
}

TEST(NativeTest, DeadNodes)
{
    Model model = built(core());
    const Offset gone = unary(model, BIYOS_OP_EXP, model.add_variable_node(0));
    const Offset kept = unary(model, BIYOS_OP_LOG, model.add_variable_node(2));
    attach(model, {0, 1}, {gone, kept});
    const std::array cleared{Index{0}};
    model.clear_functions(cleared);
    Parts parts = core();
    parts.constants[0] = 0.0;
    Model wanted = built(parts);
    attach(wanted, {1}, {unary(wanted, BIYOS_OP_LOG, wanted.add_variable_node(2))});
    EXPECT_EQ(native_text(model), native_text(wanted));
    expect_round(model);
}

TEST(NativeTest, Locale)
{
    Parts parts = empty();
    parts.variables.assign(1000, {.kind = biyos::VariableKind::Continuous, .lower = 0, .upper = 1});
    const Model model = built(parts);
    const std::locale grouped(std::locale::classic(), new Thousands);
    const std::locale previous = std::locale::global(grouped);
    const std::string text = native_text(model);
    std::locale::global(previous);
    EXPECT_NE(text.find("VARIABLES 1000\n"), std::string::npos);
    expect_round(model);
}

TEST(NativeTest, Names)
{
    Parts parts = empty();
    parts.variables = sample().variables;
    parts.name = "a b\\c";
    parts.variable_names = {.indices = {0, 1, 2}, .names = {"tab\tx", "\x7f", "\xc3\xa9t\xc3\xa9"}};
    const Model model = built(parts);
    const std::string text = native_text(model);
    EXPECT_NE(text.find("NAME a\\20b\\5Cc\n"), std::string::npos);
    EXPECT_NE(text.find("0 tab\\09x\n1 \\7F\n2 \xc3\xa9t\xc3\xa9\n"), std::string::npos);
    expect_round(model);
}

TEST(NativeTest, Numbers)
{
    Parts parts = empty();
    parts.variables = {
        {.kind = biyos::VariableKind::Continuous, .lower = -0.0, .upper = 0.1},
        {.kind = biyos::VariableKind::Continuous, .lower = -kInfinity, .upper = 1e300},
        {.kind = biyos::VariableKind::SemiContinuous, .lower = 5e-324, .upper = kInfinity},
    };
    const Model model = built(parts);
    EXPECT_NE(native_text(model).find("cont -0 0.1\ncont -inf 1e+300\nsemicont 5e-324 inf\n"),
              std::string::npos);
    expect_round(model);
}

TEST(NativeTest, MpsFiles)
{
    for (const std::string_view file : kMpsFiles)
    {
        SCOPED_TRACE(file);
        expect_round(mps_model(file));
    }
}

TEST(NativeTest, NlFiles)
{
    for (const std::string_view file : kNlFiles)
    {
        SCOPED_TRACE(file);
        expect_round(biyos::test::nl::read_sample(file));
    }
}
