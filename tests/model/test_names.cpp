#include "core/index.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using biyos::Constraint;
using biyos::Family;
using biyos::Index;
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

Model sized(Index n)
{
    const auto size = static_cast<std::size_t>(n);
    Model mdl;
    mdl.add_vars(continuous(n));
    const std::vector<double> constants(size, 0.0);
    const std::vector<Offset> starts(size + 1, 0);
    mdl.add_funs(constants, starts, {});
    const Constraint record{.function = 0, .lower = -1.0, .upper = 1.0};
    mdl.add_cons(std::vector<Constraint>(size, record));
    mdl.add_objs(std::vector<biyos::Objective>(size, biyos::default_obj(0, 0)));
    return mdl;
}

std::string held(const std::string *name)
{
    return name == nullptr ? "none" : "'" + *name + "'";
}

Index n_of(const Model &mdl, Family family)
{
    switch (family)
    {
    case Family::Variable:
        return mdl.n_vars();
    case Family::Function:
        return mdl.n_funs();
    case Family::Constraint:
        return mdl.n_cons();
    case Family::Objective:
        return mdl.n_objs();
    }
    return 0;
}

Held names_of(const Model &mdl, Family family)
{
    Held found;
    for (Index k = 0; k < n_of(mdl, family); ++k)
    {
        found.push_back(held(mdl.name_of(family, k)));
    }
    return found;
}

void name(Model &mdl, Family family, const Indices &idx, const Texts &names)
{
    mdl.set_names(family, idx, names);
}

std::string name_refusal(Model &mdl, Family family, const Indices &idx, const Texts &names)
{
    return refusal([&] { name(mdl, family, idx, names); });
}

std::string mdl_refusal(Model &mdl, std::string_view name)
{
    return refusal([&] { mdl.set_name(name); });
}

std::string prefixed(Family family, Index idx, const std::string &reason)
{
    const std::string noun(biyos::noun_of(family));
    return "invalid argument: " + noun + " " + std::to_string(idx) + ": " + reason;
}

std::string lookup_refusal(const Model &mdl, Family family, std::string_view name)
{
    return refusal([&] { mdl.idx_of(family, name); });
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

void unname(Model &mdl, Family family, const Indices &idx)
{
    mdl.del_names(family, idx);
}

}

TEST(NameTest, ModelName)
{
    Model mdl;
    EXPECT_EQ(mdl.name(), nullptr);
    mdl.set_name("plant");
    EXPECT_EQ(held(mdl.name()), "'plant'");
    mdl.set_name("\xc3\xa9t\xc3\xa9 \xf0\x9f\x98\x80");
    EXPECT_EQ(held(mdl.name()), "'\xc3\xa9t\xc3\xa9 \xf0\x9f\x98\x80'");
    mdl.del_name();
    EXPECT_EQ(mdl.name(), nullptr);
    mdl.del_name();
    EXPECT_EQ(mdl.name(), nullptr);
    mdl.set_name("plant");
    EXPECT_EQ(held(mdl.name()), "'plant'");
}

TEST(NameTest, Text)
{
    Model mdl;
    mdl.set_name("kept");
    const std::string prefix = "invalid argument: model: name ";
    EXPECT_EQ(mdl_refusal(mdl, ""), prefix + "is empty");
    EXPECT_EQ(mdl_refusal(mdl, std::string_view("\0", 1)), prefix + "holds a NUL at byte 0");
    EXPECT_EQ(mdl_refusal(mdl, std::string_view("a\xc3\xa9\0", 4)),
              prefix + "holds a NUL at byte 3");
    EXPECT_EQ(mdl_refusal(mdl, "ab\xff"), prefix + "is not valid UTF-8 at byte 2");
    EXPECT_EQ(mdl_refusal(mdl, "\xc3"), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(mdl_refusal(mdl, "x\xc0\xaf"), prefix + "is not valid UTF-8 at byte 1");
    EXPECT_EQ(mdl_refusal(mdl, "\xe2\x82\xac\xed\xa0\x80"),
              prefix + "is not valid UTF-8 at byte 3");
    EXPECT_EQ(mdl_refusal(mdl, "\xf4\x90\x80\x80"), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(mdl_refusal(mdl, "\xe2\x82 "), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(held(mdl.name()), "'kept'");
}

TEST(NameTest, Nouns)
{
    EXPECT_EQ(biyos::noun_of(Family::Variable), "variable");
    EXPECT_EQ(biyos::noun_of(Family::Function), "function");
    EXPECT_EQ(biyos::noun_of(Family::Constraint), "constraint");
    EXPECT_EQ(biyos::noun_of(Family::Objective), "objective");
}

TEST(NameTest, Unnamed)
{
    Model mdl = sized(3);
    for (const Family family : biyos::kFamilies)
    {
        name(mdl, family, {}, {});
        unname(mdl, family, {});
        EXPECT_EQ(names_of(mdl, family), Held(3, "none"));
        EXPECT_EQ(lookup_refusal(mdl, family, "x"), unresolved(family, "x", "unknown"));
        EXPECT_EQ(refusal([&] { mdl.name_of(family, 3); }), prefixed(family, 3, "unknown"));
        EXPECT_EQ(refusal([&] { mdl.name_of(family, -1); }), prefixed(family, -1, "unknown"));
    }
}

TEST(NameTest, Set)
{
    for (const Family family : biyos::kFamilies)
    {
        Model mdl = sized(3);
        name(mdl, family, {2, 0}, {"z", "x"});
        EXPECT_EQ(names_of(mdl, family), Held({"'x'", "none", "'z'"}));
        for (const Family other : biyos::kFamilies)
        {
            if (other != family)
            {
                EXPECT_EQ(names_of(mdl, other), Held(3, "none"));
            }
        }
    }
    Model mdl = sized(3);
    mdl.add_vars(continuous(2));
    name(mdl, kVariable, {4}, {"\xc3\xa9t\xc3\xa9"});
    EXPECT_EQ(held(mdl.name_of(kVariable, 3)), "none");
    EXPECT_EQ(held(mdl.name_of(kVariable, 4)), "'\xc3\xa9t\xc3\xa9'");
}

TEST(NameTest, Lookup)
{
    Model mdl = sized(3);
    Index shift = 0;
    for (const Family family : biyos::kFamilies)
    {
        name(mdl, family, {shift % 3, (shift + 1) % 3}, {"x", "y"});
        ++shift;
    }
    shift = 0;
    for (const Family family : biyos::kFamilies)
    {
        EXPECT_EQ(mdl.idx_of(family, "x"), shift % 3);
        EXPECT_EQ(mdl.idx_of(family, "y"), (shift + 1) % 3);
        ++shift;
    }
    const std::string_view nul("x\0", 2);
    EXPECT_EQ(lookup_refusal(mdl, kVariable, "X"), unresolved(kVariable, "X", "unknown"));
    EXPECT_EQ(lookup_refusal(mdl, kVariable, "x "), unresolved(kVariable, "x ", "unknown"));
    EXPECT_EQ(lookup_refusal(mdl, kVariable, ""), unresolved(kVariable, "", "unknown"));
    EXPECT_FALSE(lookup_refusal(mdl, kVariable, nul).empty());
    EXPECT_EQ(lookup_refusal(mdl, kVariable, "\xff"), unresolved(kVariable, "\xff", "unknown"));
}

TEST(NameTest, Repeat)
{
    for (const Family family : biyos::kFamilies)
    {
        Model mdl = sized(4);
        name(mdl, family, {0}, {"x"});
        name(mdl, family, {1}, {"x"});
        EXPECT_EQ(names_of(mdl, family), Held({"'x'", "'x'", "none", "none"}));
        EXPECT_EQ(lookup_refusal(mdl, family, "x"), repeated(family, "x", 2));
        name(mdl, family, {2, 3}, {"x", "y"});
        EXPECT_EQ(lookup_refusal(mdl, family, "x"), repeated(family, "x", 3));
        EXPECT_EQ(mdl.idx_of(family, "y"), 3);
        name(mdl, family, {3, 3}, {"x", "x"});
        EXPECT_EQ(names_of(mdl, family), Held(4, "'x'"));
        EXPECT_EQ(lookup_refusal(mdl, family, "x"), repeated(family, "x", 4));
        EXPECT_EQ(lookup_refusal(mdl, family, "y"), unresolved(family, "y", "unknown"));
    }
}

TEST(NameTest, Resolved)
{
    for (const Family family : biyos::kFamilies)
    {
        Model mdl = sized(3);
        name(mdl, family, {0, 1, 2}, {"x", "x", "x"});
        name(mdl, family, {0}, {"w"});
        EXPECT_EQ(lookup_refusal(mdl, family, "x"), repeated(family, "x", 2));
        unname(mdl, family, {1});
        EXPECT_EQ(mdl.idx_of(family, "x"), 2);
        EXPECT_EQ(mdl.idx_of(family, "w"), 0);
        name(mdl, family, {1}, {"x"});
        EXPECT_EQ(lookup_refusal(mdl, family, "x"), repeated(family, "x", 2));
        name(mdl, family, {2}, {"v"});
        EXPECT_EQ(mdl.idx_of(family, "x"), 1);
        name(mdl, family, {2, 0}, {"w", "v"});
        EXPECT_EQ(mdl.idx_of(family, "v"), 0);
        EXPECT_EQ(mdl.idx_of(family, "w"), 2);
    }
}

TEST(NameTest, Delete)
{
    for (const Family family : biyos::kFamilies)
    {
        Model mdl = sized(3);
        name(mdl, family, {0, 1, 2}, {"a", "b", "c"});
        const std::string *kept = mdl.name_of(family, 2);
        unname(mdl, family, {1, 1, 0});
        EXPECT_EQ(names_of(mdl, family), Held({"none", "none", "'c'"}));
        EXPECT_EQ(mdl.name_of(family, 2), kept);
        EXPECT_EQ(lookup_refusal(mdl, family, "a"), unresolved(family, "a", "unknown"));
        unname(mdl, family, {0});
        EXPECT_EQ(refusal([&] { unname(mdl, family, {2, 3}); }), prefixed(family, 3, "unknown"));
        EXPECT_EQ(refusal([&] { unname(mdl, family, {-1}); }), prefixed(family, -1, "unknown"));
        EXPECT_EQ(names_of(mdl, family), Held({"none", "none", "'c'"}));
        name(mdl, family, {1}, {"a"});
        EXPECT_EQ(mdl.idx_of(family, "a"), 1);
        EXPECT_EQ(mdl.idx_of(family, "c"), 2);
    }
}

TEST(NameTest, Rename)
{
    for (const Family family : biyos::kFamilies)
    {
        Model mdl = sized(3);
        name(mdl, family, {0}, {"x"});
        name(mdl, family, {0}, {"w"});
        EXPECT_EQ(lookup_refusal(mdl, family, "x"), unresolved(family, "x", "unknown"));
        EXPECT_EQ(mdl.idx_of(family, "w"), 0);
        name(mdl, family, {1}, {"x"});
        EXPECT_EQ(mdl.idx_of(family, "x"), 1);
        name(mdl, family, {1}, {"x"});
        EXPECT_EQ(mdl.idx_of(family, "x"), 1);
        EXPECT_EQ(names_of(mdl, family), Held({"'w'", "'x'", "none"}));
    }
}

TEST(NameTest, Swap)
{
    Model mdl = sized(3);
    name(mdl, kVariable, {0, 1, 2}, {"a", "b", "c"});
    name(mdl, kVariable, {0, 1}, {"b", "a"});
    EXPECT_EQ(names_of(mdl, kVariable), Held({"'b'", "'a'", "'c'"}));
    name(mdl, kVariable, {2, 0, 1}, {"b", "a", "c"});
    EXPECT_EQ(names_of(mdl, kVariable), Held({"'a'", "'c'", "'b'"}));
    EXPECT_EQ(mdl.idx_of(kVariable, "a"), 0);
    EXPECT_EQ(mdl.idx_of(kVariable, "b"), 2);
    EXPECT_EQ(mdl.idx_of(kVariable, "c"), 1);
}

TEST(NameTest, LastEntry)
{
    Model mdl = sized(3);
    name(mdl, kVariable, {1}, {"a"});
    name(mdl, kVariable, {0, 0}, {"a", "b"});
    EXPECT_EQ(names_of(mdl, kVariable), Held({"'b'", "'a'", "none"}));
    EXPECT_EQ(mdl.idx_of(kVariable, "a"), 1);
    name(mdl, kVariable, {2, 0, 2}, {"c", "e", "d"});
    EXPECT_EQ(names_of(mdl, kVariable), Held({"'e'", "'a'", "'d'"}));
    EXPECT_EQ(lookup_refusal(mdl, kVariable, "c"), unresolved(kVariable, "c", "unknown"));
    EXPECT_EQ(name_refusal(mdl, kVariable, {0, 0}, {"", "f"}),
              "invalid argument: variable 0: name is empty");
    EXPECT_EQ(names_of(mdl, kVariable), Held({"'e'", "'a'", "'d'"}));
}

TEST(NameTest, Refused)
{
    for (const Family family : biyos::kFamilies)
    {
        Model mdl = sized(3);
        name(mdl, family, {0}, {"x"});
        const std::string_view nul("y\0", 2);
        EXPECT_EQ(name_refusal(mdl, family, {3}, {"y"}), prefixed(family, 3, "unknown"));
        EXPECT_EQ(name_refusal(mdl, family, {-1}, {"y"}), prefixed(family, -1, "unknown"));
        EXPECT_EQ(name_refusal(mdl, family, {1}, {""}), prefixed(family, 1, "name is empty"));
        EXPECT_EQ(name_refusal(mdl, family, {2}, {nul}),
                  prefixed(family, 2, "name holds a NUL at byte 1"));
        EXPECT_EQ(name_refusal(mdl, family, {1}, {"y\xc3"}),
                  prefixed(family, 1, "name is not valid UTF-8 at byte 1"));
        EXPECT_EQ(name_refusal(mdl, family, {1, 2}, {"x", ""}),
                  prefixed(family, 2, "name is empty"));
        EXPECT_EQ(names_of(mdl, family), Held({"'x'", "none", "none"}));
        EXPECT_EQ(mdl.idx_of(family, "x"), 0);
    }
}

TEST(NameTest, Order)
{
    Model mdl = sized(3);
    EXPECT_EQ(name_refusal(mdl, kVariable, {0, 9}, {"", "y"}),
              "invalid argument: variable 0: name is empty");
    EXPECT_EQ(name_refusal(mdl, kVariable, {9, 0}, {"y", ""}),
              "invalid argument: variable 9: unknown");
    EXPECT_EQ(name_refusal(mdl, kVariable, {9}, {""}), "invalid argument: variable 9: unknown");
    EXPECT_EQ(name_refusal(mdl, kVariable, {0, 1}, {"x", ""}),
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
            EXPECT_EQ(made->idx_of(family, "x"), 0);
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
        EXPECT_EQ(copy.idx_of(family, "z"), 2);
        EXPECT_EQ(names_of(assigned, family), Held({"'x'", "'z'", "'z'"}));
    }
}

TEST(NameTest, Stable)
{
    Model mdl = sized(2);
    name(mdl, kVariable, {0}, {"x"});
    const std::string *kept = mdl.name_of(kVariable, 0);
    mdl.add_vars(continuous(1000));
    Indices idx;
    std::vector<std::string> texts;
    for (Index k = 1; k < mdl.n_vars(); ++k)
    {
        idx.push_back(k);
        texts.push_back("v" + std::to_string(k));
    }
    name(mdl, kVariable, idx, Texts(texts.begin(), texts.end()));
    name(mdl, kVariable, {1, 2}, {"x", "x"});
    EXPECT_EQ(name_refusal(mdl, kVariable, {0, 3}, {"y", ""}),
              "invalid argument: variable 3: name is empty");
    unname(mdl, kVariable, {1, 2});
    name(mdl, Family::Constraint, {0}, {"x"});
    const Model copy = mdl;
    mdl.set_name("plant");
    EXPECT_EQ(mdl.name_of(kVariable, 0), kept);
    EXPECT_EQ(*kept, "x");
    EXPECT_EQ(mdl.idx_of(kVariable, "x"), 0);
    EXPECT_EQ(mdl.idx_of(kVariable, "v1001"), 1001);
}
