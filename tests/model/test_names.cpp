#include "core/index.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using biyos::Complement;
using biyos::ConeKind;
using biyos::Constraint;
using biyos::Family;
using biyos::Index;
using biyos::Indicator;
using biyos::Model;
using biyos::Offset;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::refusal;

using Indices = std::vector<Index>;
using Texts = std::vector<std::string_view>;
using Held = std::vector<std::string>;

constexpr Family kVariable = Family::Variable;

std::vector<Variable> continuous(Index n)
{
    const Variable free = biyos::widest(VariableKind::Continuous);
    return std::vector<Variable>(static_cast<std::size_t>(n), free);
}

void add_cones(Model &model, std::size_t size)
{
    std::vector<Offset> starts(size + 1);
    std::iota(starts.begin(), starts.end(), Offset{0});
    const std::vector<ConeKind> kinds(size, ConeKind::Nonnegative);
    const std::vector<Offset> none(size + 1, 0);
    model.add_cones(kinds, starts, std::vector<Index>(size, 0), none, {});
}

Model sized(Index n)
{
    const auto size = static_cast<std::size_t>(n);
    Model model;
    model.add_variables(continuous(n));
    const std::vector<double> constants(size, 0.0);
    const std::vector<Offset> starts(size + 1, 0);
    model.add_functions(constants, starts, {});
    const Constraint record{.function = 0, .lower = -1.0, .upper = 1.0};
    model.add_constraints(std::vector<Constraint>(size, record));
    model.add_objectives(std::vector<biyos::Objective>(size, biyos::default_objective(0, 0)));
    const Index first = 0;
    const Variable binary = biyos::widest(VariableKind::Binary);
    model.set_variables(std::span(&first, 1), std::span(&binary, 1));
    const Indicator indicator{
        .variable = 0,
        .value = 1,
        .function = 0,
        .lower = 0.0,
        .upper = 1.0,
    };
    model.add_indicators(std::vector<Indicator>(size, indicator));
    const std::vector<biyos::SosKind> kinds(size, biyos::SosKind::Sos1);
    model.add_sos(kinds, starts, {});
    add_cones(model, size);
    const Complement complement{.function = 0, .partner = 0, .lower = 0.0, .upper = 1.0};
    model.add_complements(std::vector<Complement>(size, complement));
    return model;
}

std::string held(const std::string *name)
{
    return name == nullptr ? "none" : "'" + *name + "'";
}

Index n_of(const Model &model, Family family)
{
    switch (family)
    {
    case Family::Variable:
        return model.n_variables();
    case Family::Function:
        return model.n_functions();
    case Family::Constraint:
        return model.n_constraints();
    case Family::Objective:
        return model.n_objectives();
    case Family::Indicator:
        return model.n_indicators();
    case Family::Sos:
        return model.n_sos();
    case Family::Cone:
        return model.n_cones();
    case Family::Complement:
        return model.n_complements();
    }
    return 0;
}

Held names_of(const Model &model, Family family)
{
    Held found;
    for (Index k = 0; k < n_of(model, family); ++k)
    {
        found.push_back(held(model.name_of(family, k)));
    }
    return found;
}

void name(Model &model, Family family, const Indices &index, const Texts &names)
{
    model.set_names(family, index, names);
}

std::string name_refusal(Model &model, Family family, const Indices &index, const Texts &names)
{
    return refusal([&] { name(model, family, index, names); });
}

std::string model_refusal(Model &model, std::string_view name)
{
    return refusal([&] { model.set_name(name); });
}

std::string prefixed(Family family, Index index, const std::string &reason)
{
    const std::string noun(biyos::noun_of(family));
    return "invalid argument: " + noun + " " + std::to_string(index) + ": " + reason;
}

std::string lookup_refusal(const Model &model, Family family, std::string_view name)
{
    return refusal([&] { model.index_of(family, name); });
}

std::string unresolved(Family family, const std::string &name, const std::string &reason)
{
    const std::string noun(biyos::noun_of(family));
    return "invalid argument: " + noun + " '" + name + "': " + reason;
}

std::string repeated(Family family, const std::string &name, Index n)
{
    const std::string noun(biyos::noun_of(family));
    return unresolved(family, name, "names " + std::to_string(n) + " " + noun + "s");
}

void unname(Model &model, Family family, const Indices &index)
{
    model.delete_names(family, index);
}

}

TEST(NameTest, ModelName)
{
    Model model;
    EXPECT_EQ(model.name(), nullptr);
    model.set_name("plant");
    EXPECT_EQ(held(model.name()), "'plant'");
    model.set_name("\xc3\xa9t\xc3\xa9 \xf0\x9f\x98\x80");
    EXPECT_EQ(held(model.name()), "'\xc3\xa9t\xc3\xa9 \xf0\x9f\x98\x80'");
    model.delete_name();
    EXPECT_EQ(model.name(), nullptr);
    model.delete_name();
    EXPECT_EQ(model.name(), nullptr);
    model.set_name("plant");
    EXPECT_EQ(held(model.name()), "'plant'");
}

TEST(NameTest, Text)
{
    Model model;
    model.set_name("kept");
    const std::string prefix = "invalid argument: model: name ";
    EXPECT_EQ(model_refusal(model, ""), prefix + "is empty");
    EXPECT_EQ(model_refusal(model, std::string_view("\0", 1)), prefix + "holds a NUL at byte 0");
    EXPECT_EQ(model_refusal(model, std::string_view("a\xc3\xa9\0", 4)),
              prefix + "holds a NUL at byte 3");
    EXPECT_EQ(model_refusal(model, "ab\xff"), prefix + "is not valid UTF-8 at byte 2");
    EXPECT_EQ(model_refusal(model, "\xc3"), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(model_refusal(model, "x\xc0\xaf"), prefix + "is not valid UTF-8 at byte 1");
    EXPECT_EQ(model_refusal(model, "\xe2\x82\xac\xed\xa0\x80"),
              prefix + "is not valid UTF-8 at byte 3");
    EXPECT_EQ(model_refusal(model, "\xf4\x90\x80\x80"), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(model_refusal(model, "\xe2\x82 "), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(held(model.name()), "'kept'");
}

TEST(NameTest, Nouns)
{
    EXPECT_EQ(biyos::noun_of(Family::Variable), "variable");
    EXPECT_EQ(biyos::noun_of(Family::Function), "function");
    EXPECT_EQ(biyos::noun_of(Family::Constraint), "constraint");
    EXPECT_EQ(biyos::noun_of(Family::Objective), "objective");
    EXPECT_EQ(biyos::noun_of(Family::Indicator), "indicator");
    EXPECT_EQ(biyos::noun_of(Family::Sos), "set");
    EXPECT_EQ(biyos::noun_of(Family::Cone), "cone");
    EXPECT_EQ(biyos::noun_of(Family::Complement), "complement");
}

TEST(NameTest, Unnamed)
{
    Model model = sized(3);
    for (const Family family : biyos::kFamilies)
    {
        name(model, family, {}, {});
        unname(model, family, {});
        EXPECT_EQ(names_of(model, family), Held(3, "none"));
        EXPECT_EQ(lookup_refusal(model, family, "x"), unresolved(family, "x", "unknown"));
        EXPECT_EQ(refusal([&] { model.name_of(family, 3); }), prefixed(family, 3, "unknown"));
        EXPECT_EQ(refusal([&] { model.name_of(family, -1); }), prefixed(family, -1, "unknown"));
    }
}

TEST(NameTest, Set)
{
    for (const Family family : biyos::kFamilies)
    {
        Model model = sized(3);
        name(model, family, {2, 0}, {"z", "x"});
        EXPECT_EQ(names_of(model, family), Held({"'x'", "none", "'z'"}));
        for (const Family other : biyos::kFamilies)
        {
            if (other != family)
            {
                EXPECT_EQ(names_of(model, other), Held(3, "none"));
            }
        }
    }
    Model model = sized(3);
    model.add_variables(continuous(2));
    name(model, kVariable, {4}, {"\xc3\xa9t\xc3\xa9"});
    EXPECT_EQ(held(model.name_of(kVariable, 3)), "none");
    EXPECT_EQ(held(model.name_of(kVariable, 4)), "'\xc3\xa9t\xc3\xa9'");
}

TEST(NameTest, Lookup)
{
    Model model = sized(3);
    Index shift = 0;
    for (const Family family : biyos::kFamilies)
    {
        name(model, family, {shift % 3, (shift + 1) % 3}, {"x", "y"});
        ++shift;
    }
    shift = 0;
    for (const Family family : biyos::kFamilies)
    {
        EXPECT_EQ(model.index_of(family, "x"), shift % 3);
        EXPECT_EQ(model.index_of(family, "y"), (shift + 1) % 3);
        ++shift;
    }
    const std::string_view nul("x\0", 2);
    EXPECT_EQ(lookup_refusal(model, kVariable, "X"), unresolved(kVariable, "X", "unknown"));
    EXPECT_EQ(lookup_refusal(model, kVariable, "x "), unresolved(kVariable, "x ", "unknown"));
    EXPECT_EQ(lookup_refusal(model, kVariable, ""), unresolved(kVariable, "", "unknown"));
    EXPECT_FALSE(lookup_refusal(model, kVariable, nul).empty());
    EXPECT_EQ(lookup_refusal(model, kVariable, "\xff"), unresolved(kVariable, "\xff", "unknown"));
}

TEST(NameTest, Repeat)
{
    for (const Family family : biyos::kFamilies)
    {
        Model model = sized(4);
        name(model, family, {0}, {"x"});
        name(model, family, {1}, {"x"});
        EXPECT_EQ(names_of(model, family), Held({"'x'", "'x'", "none", "none"}));
        EXPECT_EQ(lookup_refusal(model, family, "x"), repeated(family, "x", 2));
        name(model, family, {2, 3}, {"x", "y"});
        EXPECT_EQ(lookup_refusal(model, family, "x"), repeated(family, "x", 3));
        EXPECT_EQ(model.index_of(family, "y"), 3);
        name(model, family, {3, 3}, {"x", "x"});
        EXPECT_EQ(names_of(model, family), Held(4, "'x'"));
        EXPECT_EQ(lookup_refusal(model, family, "x"), repeated(family, "x", 4));
        EXPECT_EQ(lookup_refusal(model, family, "y"), unresolved(family, "y", "unknown"));
    }
}

TEST(NameTest, Resolved)
{
    for (const Family family : biyos::kFamilies)
    {
        Model model = sized(3);
        name(model, family, {0, 1, 2}, {"x", "x", "x"});
        name(model, family, {0}, {"w"});
        EXPECT_EQ(lookup_refusal(model, family, "x"), repeated(family, "x", 2));
        unname(model, family, {1});
        EXPECT_EQ(model.index_of(family, "x"), 2);
        EXPECT_EQ(model.index_of(family, "w"), 0);
        name(model, family, {1}, {"x"});
        EXPECT_EQ(lookup_refusal(model, family, "x"), repeated(family, "x", 2));
        name(model, family, {2}, {"v"});
        EXPECT_EQ(model.index_of(family, "x"), 1);
        name(model, family, {2, 0}, {"w", "v"});
        EXPECT_EQ(model.index_of(family, "v"), 0);
        EXPECT_EQ(model.index_of(family, "w"), 2);
    }
}

TEST(NameTest, Delete)
{
    for (const Family family : biyos::kFamilies)
    {
        Model model = sized(3);
        name(model, family, {0, 1, 2}, {"a", "b", "c"});
        const std::string *kept = model.name_of(family, 2);
        unname(model, family, {1, 1, 0});
        EXPECT_EQ(names_of(model, family), Held({"none", "none", "'c'"}));
        EXPECT_EQ(model.name_of(family, 2), kept);
        EXPECT_EQ(lookup_refusal(model, family, "a"), unresolved(family, "a", "unknown"));
        unname(model, family, {0});
        EXPECT_EQ(refusal([&] { unname(model, family, {2, 3}); }), prefixed(family, 3, "unknown"));
        EXPECT_EQ(refusal([&] { unname(model, family, {-1}); }), prefixed(family, -1, "unknown"));
        EXPECT_EQ(names_of(model, family), Held({"none", "none", "'c'"}));
        name(model, family, {1}, {"a"});
        EXPECT_EQ(model.index_of(family, "a"), 1);
        EXPECT_EQ(model.index_of(family, "c"), 2);
    }
}

TEST(NameTest, Rename)
{
    for (const Family family : biyos::kFamilies)
    {
        Model model = sized(3);
        name(model, family, {0}, {"x"});
        name(model, family, {0}, {"w"});
        EXPECT_EQ(lookup_refusal(model, family, "x"), unresolved(family, "x", "unknown"));
        EXPECT_EQ(model.index_of(family, "w"), 0);
        name(model, family, {1}, {"x"});
        EXPECT_EQ(model.index_of(family, "x"), 1);
        name(model, family, {1}, {"x"});
        EXPECT_EQ(model.index_of(family, "x"), 1);
        EXPECT_EQ(names_of(model, family), Held({"'w'", "'x'", "none"}));
    }
}

TEST(NameTest, Swap)
{
    Model model = sized(3);
    name(model, kVariable, {0, 1, 2}, {"a", "b", "c"});
    name(model, kVariable, {0, 1}, {"b", "a"});
    EXPECT_EQ(names_of(model, kVariable), Held({"'b'", "'a'", "'c'"}));
    name(model, kVariable, {2, 0, 1}, {"b", "a", "c"});
    EXPECT_EQ(names_of(model, kVariable), Held({"'a'", "'c'", "'b'"}));
    EXPECT_EQ(model.index_of(kVariable, "a"), 0);
    EXPECT_EQ(model.index_of(kVariable, "b"), 2);
    EXPECT_EQ(model.index_of(kVariable, "c"), 1);
}

TEST(NameTest, LastEntry)
{
    Model model = sized(3);
    name(model, kVariable, {1}, {"a"});
    name(model, kVariable, {0, 0}, {"a", "b"});
    EXPECT_EQ(names_of(model, kVariable), Held({"'b'", "'a'", "none"}));
    EXPECT_EQ(model.index_of(kVariable, "a"), 1);
    name(model, kVariable, {2, 0, 2}, {"c", "e", "d"});
    EXPECT_EQ(names_of(model, kVariable), Held({"'e'", "'a'", "'d'"}));
    EXPECT_EQ(lookup_refusal(model, kVariable, "c"), unresolved(kVariable, "c", "unknown"));
    EXPECT_EQ(name_refusal(model, kVariable, {0, 0}, {"", "f"}),
              "invalid argument: variable 0: name is empty");
    EXPECT_EQ(names_of(model, kVariable), Held({"'e'", "'a'", "'d'"}));
}

TEST(NameTest, Refused)
{
    for (const Family family : biyos::kFamilies)
    {
        Model model = sized(3);
        name(model, family, {0}, {"x"});
        const std::string_view nul("y\0", 2);
        EXPECT_EQ(name_refusal(model, family, {3}, {"y"}), prefixed(family, 3, "unknown"));
        EXPECT_EQ(name_refusal(model, family, {-1}, {"y"}), prefixed(family, -1, "unknown"));
        EXPECT_EQ(name_refusal(model, family, {1}, {""}), prefixed(family, 1, "name is empty"));
        EXPECT_EQ(name_refusal(model, family, {2}, {nul}),
                  prefixed(family, 2, "name holds a NUL at byte 1"));
        EXPECT_EQ(name_refusal(model, family, {1}, {"y\xc3"}),
                  prefixed(family, 1, "name is not valid UTF-8 at byte 1"));
        EXPECT_EQ(name_refusal(model, family, {1, 2}, {"x", ""}),
                  prefixed(family, 2, "name is empty"));
        EXPECT_EQ(names_of(model, family), Held({"'x'", "none", "none"}));
        EXPECT_EQ(model.index_of(family, "x"), 0);
    }
}

TEST(NameTest, Order)
{
    Model model = sized(3);
    EXPECT_EQ(name_refusal(model, kVariable, {0, 9}, {"", "y"}),
              "invalid argument: variable 0: name is empty");
    EXPECT_EQ(name_refusal(model, kVariable, {9, 0}, {"y", ""}),
              "invalid argument: variable 9: unknown");
    EXPECT_EQ(name_refusal(model, kVariable, {9}, {""}), "invalid argument: variable 9: unknown");
    EXPECT_EQ(name_refusal(model, kVariable, {0, 1}, {"x", ""}),
              "invalid argument: variable 1: name is empty");
}

TEST(NameTest, Copied)
{
    Model source = sized(3);
    source.set_name("plant");
    for (const Family family : biyos::kFamilies)
    {
        name(source, family, {0, 1, 2}, {"x", "z", "z"});
    }
    Model copy = source;
    Model assigned;
    assigned = source;
    for (const Model *made : {&copy, &assigned})
    {
        EXPECT_EQ(held(made->name()), "'plant'");
        for (const Family family : biyos::kFamilies)
        {
            EXPECT_EQ(names_of(*made, family), Held({"'x'", "'z'", "'z'"}));
            EXPECT_EQ(made->index_of(family, "x"), 0);
            EXPECT_EQ(lookup_refusal(*made, family, "z"), repeated(family, "z", 2));
            EXPECT_NE(made->name_of(family, 0), source.name_of(family, 0));
            EXPECT_EQ(made->name_of(family, 1), made->name_of(family, 2));
        }
    }
    for (const Family family : biyos::kFamilies)
    {
        name(copy, family, {0, 1}, {"y", "x"});
        EXPECT_EQ(names_of(source, family), Held({"'x'", "'z'", "'z'"}));
        EXPECT_EQ(lookup_refusal(source, family, "y"), unresolved(family, "y", "unknown"));
    }
    source = Model();
    for (const Family family : biyos::kFamilies)
    {
        EXPECT_EQ(names_of(copy, family), Held({"'y'", "'x'", "'z'"}));
        EXPECT_EQ(copy.index_of(family, "z"), 2);
        EXPECT_EQ(names_of(assigned, family), Held({"'x'", "'z'", "'z'"}));
    }
}

TEST(NameTest, Stable)
{
    Model model = sized(2);
    name(model, kVariable, {0}, {"x"});
    const std::string *kept = model.name_of(kVariable, 0);
    model.add_variables(continuous(1000));
    Indices index;
    std::vector<std::string> texts;
    for (Index k = 1; k < model.n_variables(); ++k)
    {
        index.push_back(k);
        texts.push_back("v" + std::to_string(k));
    }
    name(model, kVariable, index, Texts(texts.begin(), texts.end()));
    name(model, kVariable, {1, 2}, {"x", "x"});
    EXPECT_EQ(name_refusal(model, kVariable, {0, 3}, {"y", ""}),
              "invalid argument: variable 3: name is empty");
    unname(model, kVariable, {1, 2});
    name(model, Family::Constraint, {0}, {"x"});
    const Model copy = model;
    model.set_name("plant");
    EXPECT_EQ(model.name_of(kVariable, 0), kept);
    EXPECT_EQ(*kept, "x");
    EXPECT_EQ(model.index_of(kVariable, "x"), 0);
    EXPECT_EQ(model.index_of(kVariable, "v1001"), 1001);
}
