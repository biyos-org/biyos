#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

const std::string kLocked = "invalid argument: model: is read-only while delivering an event";

using Indices = std::vector<BIYOS_index>;
using Texts = std::vector<const char *>;

struct Calls
{
    std::string noun;
    BIYOS_status (*set)(BIYOS_model *, BIYOS_index, const BIYOS_index *, const char *const *);
    BIYOS_status (*get)(const BIYOS_model *, BIYOS_index, const BIYOS_index *, const char **);
    BIYOS_status (*find)(const BIYOS_model *, BIYOS_index, const char *const *, BIYOS_index *);
    BIYOS_status (*del)(BIYOS_model *, BIYOS_index, const BIYOS_index *);
};

const std::array<Calls, 4> kFamilies{{
    {
        .noun = "variable",
        .set = BIYOS_setvarname,
        .get = BIYOS_getvarname,
        .find = BIYOS_getvaridx,
        .del = BIYOS_delvarname,
    },
    {
        .noun = "function",
        .set = BIYOS_setfunname,
        .get = BIYOS_getfunname,
        .find = BIYOS_getfunidx,
        .del = BIYOS_delfunname,
    },
    {
        .noun = "constraint",
        .set = BIYOS_setconname,
        .get = BIYOS_getconname,
        .find = BIYOS_getconidx,
        .del = BIYOS_delconname,
    },
    {
        .noun = "objective",
        .set = BIYOS_setobjname,
        .get = BIYOS_getobjname,
        .find = BIYOS_getobjidx,
        .del = BIYOS_delobjname,
    },
}};

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
};

BIYOS_status get_one(const Calls &family,
                     const BIYOS_model *mdl,
                     BIYOS_index idx,
                     const char **name)
{
    return family.get(mdl, 1, &idx, name);
}

BIYOS_status find_one(const Calls &family,
                      const BIYOS_model *mdl,
                      const char *name,
                      BIYOS_index *idx)
{
    return family.find(mdl, 1, &name, idx);
}

void add_items(const Model &mdl, BIYOS_index n)
{
    BIYOS_model *held = mdl.get();
    const Indices funs(static_cast<std::size_t>(n), 0);
    ASSERT_EQ(BIYOS_addvars(held, n, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfuns(held, n, nullptr, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcons(held, n, funs.data(), nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addobjs(held, n, funs.data(), nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
}

BIYOS_status name(const Calls &family, const Model &mdl, const Indices &idx, const Texts &names)
{
    const auto n = static_cast<BIYOS_index>(idx.size());
    return family.set(mdl.get(), n, idx.data(), names.data());
}

std::string name_of(const Calls &family, const Model &mdl, BIYOS_index idx)
{
    const char *found = nullptr;
    EXPECT_EQ(get_one(family, mdl.get(), idx, &found), BIYOS_OK);
    return found == nullptr ? "null" : found;
}

BIYOS_index idx_of(const Calls &family, const Model &mdl, const char *name)
{
    BIYOS_index idx = -1;
    EXPECT_EQ(find_one(family, mdl.get(), name, &idx), BIYOS_OK);
    return idx;
}

std::vector<std::string> names_of(const Calls &family, const Model &mdl, BIYOS_index n)
{
    std::vector<std::string> found;
    for (BIYOS_index k = 0; k < n; ++k)
    {
        found.push_back(name_of(family, mdl, k));
    }
    return found;
}

std::string mdl_name(const Model &mdl)
{
    const char *name = nullptr;
    EXPECT_EQ(BIYOS_getmdlname(mdl.get(), &name), BIYOS_OK);
    return name == nullptr ? "null" : name;
}

std::string refused(const Calls &family, BIYOS_index idx, const std::string &reason)
{
    return "invalid argument: " + family.noun + " " + std::to_string(idx) + ": " + reason;
}

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(BIYOS_lasterror(attempt.env));
}

void rename(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *mdl = attempt.model;
    const BIYOS_index idx = 0;
    const char *name = "w";
    const char *found = nullptr;
    BIYOS_index at = -1;
    note(attempt, BIYOS_setmdlname(mdl, name));
    note(attempt, BIYOS_delmdlname(mdl));
    note(attempt, BIYOS_getmdlname(mdl, &found));
    for (const Calls &family : kFamilies)
    {
        note(attempt, family.set(mdl, 1, &idx, &name));
        note(attempt, get_one(family, mdl, 0, &found));
        note(attempt, find_one(family, mdl, "x", &at));
        note(attempt, family.del(mdl, 1, &idx));
    }
}

}

TEST(CApiTest, ModelName)
{
    const Env env;
    const Model mdl(env);
    EXPECT_EQ(mdl_name(mdl), "");
    ASSERT_EQ(BIYOS_setmdlname(mdl.get(), "plant"), BIYOS_OK);
    EXPECT_EQ(mdl_name(mdl), "plant");
    ASSERT_EQ(BIYOS_setmdlname(mdl.get(), "\xc3\xa9t\xc3\xa9"), BIYOS_OK);
    EXPECT_EQ(mdl_name(mdl), "\xc3\xa9t\xc3\xa9");
    EXPECT_EQ(BIYOS_setmdlname(mdl.get(), ""), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: model: name is empty");
    EXPECT_EQ(BIYOS_setmdlname(mdl.get(), "ab\xc3"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: model: name is not valid UTF-8 at byte 2");
    EXPECT_EQ(BIYOS_setmdlname(mdl.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
    EXPECT_EQ(BIYOS_getmdlname(mdl.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
    EXPECT_EQ(mdl_name(mdl), "\xc3\xa9t\xc3\xa9");
    const char *name = "kept";
    EXPECT_EQ(BIYOS_setmdlname(nullptr, "plant"), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getmdlname(nullptr, &name), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delmdlname(nullptr), BIYOS_ERR_BADARG);
    EXPECT_STREQ(name, "kept");
    EXPECT_EQ(BIYOS_delmdlname(mdl.get()), BIYOS_OK);
    EXPECT_EQ(mdl_name(mdl), "");
    EXPECT_EQ(BIYOS_delmdlname(mdl.get()), BIYOS_OK);
    EXPECT_EQ(mdl_name(mdl), "");
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
}

TEST(CApiTest, FamilyNames)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model mdl(env);
        add_items(mdl, 3);
        EXPECT_EQ(names_of(family, mdl, 3), std::vector<std::string>(3, ""));
        ASSERT_EQ(name(family, mdl, {2, 0}, {"z", "x"}), BIYOS_OK);
        EXPECT_EQ(names_of(family, mdl, 3), std::vector<std::string>({"x", "", "z"}));
        EXPECT_EQ(idx_of(family, mdl, "z"), 2);
        EXPECT_EQ(idx_of(family, mdl, "x"), 0);
        ASSERT_EQ(name(family, mdl, {0, 2, 1, 1}, {"z", "x", "w", "y"}), BIYOS_OK);
        EXPECT_EQ(names_of(family, mdl, 3), std::vector<std::string>({"z", "y", "x"}));
        EXPECT_EQ(idx_of(family, mdl, "y"), 1);
        EXPECT_EQ(env.message(), "");
        for (const Calls &other : kFamilies)
        {
            if (other.noun != family.noun)
            {
                EXPECT_EQ(names_of(other, mdl, 3), std::vector<std::string>(3, ""));
            }
        }
    }
}

TEST(CApiTest, SharedName)
{
    const Env env;
    const Model mdl(env);
    add_items(mdl, 4);
    BIYOS_index idx = 0;
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, mdl, {idx}, {"x"}), BIYOS_OK);
        ++idx;
    }
    idx = 0;
    for (const Calls &family : kFamilies)
    {
        EXPECT_EQ(idx_of(family, mdl, "x"), idx);
        ++idx;
    }
}

TEST(CApiTest, NameLifetime)
{
    const Env env;
    const Model mdl(env);
    add_items(mdl, 2);
    ASSERT_EQ(BIYOS_setmdlname(mdl.get(), "plant"), BIYOS_OK);
    std::vector<const char *> kept;
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, mdl, {0}, {"x"}), BIYOS_OK);
        const char *found = nullptr;
        ASSERT_EQ(get_one(family, mdl.get(), 0, &found), BIYOS_OK);
        kept.push_back(found);
    }
    const char *title = nullptr;
    ASSERT_EQ(BIYOS_getmdlname(mdl.get(), &title), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(mdl.get(), 499, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    std::vector<std::string> texts;
    Indices idx;
    for (BIYOS_index k = 1; k <= 500; ++k)
    {
        idx.push_back(k);
        texts.push_back("v" + std::to_string(k));
    }
    Texts names;
    for (const std::string &text : texts)
    {
        names.push_back(text.c_str());
    }
    const Calls &vars = kFamilies[0];
    ASSERT_EQ(name(vars, mdl, idx, names), BIYOS_OK);
    ASSERT_EQ(name(vars, mdl, {2, 1}, {"v1", "v2"}), BIYOS_OK);
    for (const Calls &family : kFamilies)
    {
        EXPECT_EQ(name(family, mdl, {0}, {""}), BIYOS_ERR_BADARG);
        ASSERT_EQ(name(family, mdl, {1}, {"y"}), BIYOS_OK);
    }
    EXPECT_EQ(name(vars, mdl, {0, 1}, {"v3", ""}), BIYOS_ERR_BADARG);
    EXPECT_EQ(kFamilies[0].del(mdl.get(), 1, &idx[9]), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlname(mdl.get(), ""), BIYOS_ERR_BADARG);
    BIYOS_model *copy = nullptr;
    ASSERT_EQ(BIYOS_copymdl(mdl.get(), env.get(), &copy), BIYOS_OK);
    const char *copied = nullptr;
    const BIYOS_index zero = 0;
    ASSERT_EQ(BIYOS_getvarname(copy, 1, &zero, &copied), BIYOS_OK);
    EXPECT_NE(copied, kept[0]);
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
    EXPECT_EQ(idx_of(vars, mdl, "v500"), 500);
    EXPECT_STREQ(title, "plant");
    for (std::size_t k = 0; k < kFamilies.size(); ++k)
    {
        EXPECT_STREQ(kept[k], "x");
        const char *again = nullptr;
        ASSERT_EQ(get_one(kFamilies[k], mdl.get(), 0, &again), BIYOS_OK);
        EXPECT_EQ(again, kept[k]);
    }
}

TEST(CApiTest, CopiedName)
{
    const Env env;
    BIYOS_model *source = nullptr;
    ASSERT_EQ(BIYOS_newmdl(env.get(), &source), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvars(source, 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index idx = 0;
    const char *name = "x";
    ASSERT_EQ(BIYOS_setvarname(source, 1, &idx, &name), BIYOS_OK);
    BIYOS_model *copy = nullptr;
    ASSERT_EQ(BIYOS_copymdl(source, env.get(), &copy), BIYOS_OK);
    const char *found = nullptr;
    ASSERT_EQ(BIYOS_getvarname(copy, 1, &idx, &found), BIYOS_OK);
    ASSERT_EQ(BIYOS_freemdl(source), BIYOS_OK);
    EXPECT_STREQ(found, "x");
    BIYOS_index at = -1;
    EXPECT_EQ(BIYOS_getvaridx(copy, 1, &name, &at), BIYOS_OK);
    EXPECT_EQ(at, 0);
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
}

TEST(CApiTest, MissingName)
{
    const Env env;
    const Model mdl(env);
    add_items(mdl, 2);
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, mdl, {0}, {family.noun.c_str()}), BIYOS_OK);
    }
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const std::string unknown = "invalid argument: " + family.noun + " '";
        const char *other = family.noun == "variable" ? "function" : "variable";
        BIYOS_index idx = -7;
        EXPECT_EQ(find_one(family, mdl.get(), "w", &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "w': unknown");
        EXPECT_EQ(find_one(family, mdl.get(), "", &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "': unknown");
        EXPECT_EQ(find_one(family, mdl.get(), other, &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + other + "': unknown");
        EXPECT_EQ(find_one(family, mdl.get(), "\xff", &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "\xff': unknown");
        EXPECT_EQ(family.find(mdl.get(), 1, nullptr, &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
        EXPECT_EQ(find_one(family, mdl.get(), nullptr, &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': entry 0 is null");
        EXPECT_EQ(find_one(family, mdl.get(), "w", nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "w': unknown");
        EXPECT_EQ(find_one(family, mdl.get(), family.noun.c_str(), nullptr), BIYOS_OK);
        EXPECT_EQ(find_one(family, nullptr, family.noun.c_str(), &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(idx, -7);
        ASSERT_EQ(name(family, mdl, {0}, {"renamed"}), BIYOS_OK);
        EXPECT_EQ(find_one(family, mdl.get(), family.noun.c_str(), &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + family.noun + "': unknown");
        EXPECT_EQ(idx, -7);
    }
}

TEST(CApiTest, NullNames)
{
    const Env env;
    const Model mdl(env);
    add_items(mdl, 3);
    BIYOS_model *held = mdl.get();
    const BIYOS_index idx[] = {0, 1};
    const char *names[] = {"x", nullptr};
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        EXPECT_EQ(family.set(held, 0, nullptr, nullptr), BIYOS_OK);
        EXPECT_EQ(family.set(held, 2, nullptr, names), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
        EXPECT_EQ(family.set(held, 2, idx, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
        EXPECT_EQ(family.set(held, 2, idx, names), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': entry 1 is null");
        EXPECT_EQ(get_one(family, held, 0, nullptr), BIYOS_OK);
        EXPECT_EQ(get_one(family, held, 3, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 3, "unknown"));
        EXPECT_EQ(family.set(nullptr, 1, idx, names), BIYOS_ERR_BADARG);
        EXPECT_EQ(name_of(family, mdl, 0), "");
    }
}

TEST(CApiTest, RepeatName)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model mdl(env);
        add_items(mdl, 3);
        ASSERT_EQ(name(family, mdl, {0, 1, 2}, {"c", "c", "c"}), BIYOS_OK);
        EXPECT_EQ(names_of(family, mdl, 3), std::vector<std::string>(3, "c"));
        BIYOS_index idx = -7;
        const std::string repeated = "invalid argument: " + family.noun + " 'c': names ";
        EXPECT_EQ(find_one(family, mdl.get(), "c", &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), repeated + "3 " + family.noun + "s");
        ASSERT_EQ(name(family, mdl, {0}, {"d"}), BIYOS_OK);
        EXPECT_EQ(find_one(family, mdl.get(), "c", &idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), repeated + "2 " + family.noun + "s");
        EXPECT_EQ(idx, -7);
        const BIYOS_index second = 1;
        ASSERT_EQ(family.del(mdl.get(), 1, &second), BIYOS_OK);
        EXPECT_EQ(idx_of(family, mdl, "c"), 2);
        EXPECT_EQ(idx_of(family, mdl, "d"), 0);
    }
}

TEST(CApiTest, DeletedName)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model mdl(env);
        add_items(mdl, 3);
        ASSERT_EQ(name(family, mdl, {0, 1, 2}, {"a", "b", "c"}), BIYOS_OK);
        const BIYOS_index idx[] = {1, 0, 1};
        EXPECT_EQ(family.del(mdl.get(), 3, idx), BIYOS_OK);
        EXPECT_EQ(names_of(family, mdl, 3), std::vector<std::string>({"", "", "c"}));
        EXPECT_EQ(family.del(mdl.get(), 0, nullptr), BIYOS_OK);
        EXPECT_EQ(family.del(mdl.get(), 1, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
        EXPECT_EQ(family.del(mdl.get(), -1, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
        const BIYOS_index outside[] = {2, 3};
        EXPECT_EQ(family.del(mdl.get(), 2, outside), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 3, "unknown"));
        EXPECT_EQ(family.del(nullptr, 1, idx), BIYOS_ERR_BADARG);
        EXPECT_EQ(names_of(family, mdl, 3), std::vector<std::string>({"", "", "c"}));
        BIYOS_index found = -7;
        EXPECT_EQ(find_one(family, mdl.get(), "a", &found), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: " + family.noun + " 'a': unknown");
        ASSERT_EQ(name(family, mdl, {1}, {"a"}), BIYOS_OK);
        EXPECT_EQ(idx_of(family, mdl, "a"), 1);
    }
}

TEST(CApiTest, NameList)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model mdl(env);
        add_items(mdl, 13);
        ASSERT_EQ(name(family, mdl, {0, 4, 10}, {"a", "e", "k"}), BIYOS_OK);
        const BIYOS_index idx[] = {0, 4, 10, 12, 4};
        std::array<const char *, 5> found{};
        ASSERT_EQ(family.get(mdl.get(), 5, idx, found.data()), BIYOS_OK);
        const std::vector<std::string> read(found.begin(), found.end());
        EXPECT_EQ(read, std::vector<std::string>({"a", "e", "k", "", "e"}));
        EXPECT_EQ(found[1], found[4]);
        EXPECT_EQ(family.get(mdl.get(), 0, nullptr, nullptr), BIYOS_OK);
        EXPECT_EQ(family.get(mdl.get(), 5, idx, nullptr), BIYOS_OK);
        const BIYOS_index outside[] = {0, 13};
        std::array<const char *, 2> kept{"kept", "kept"};
        EXPECT_EQ(family.get(mdl.get(), 2, outside, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 13, "unknown"));
        EXPECT_STREQ(kept[0], "kept");
        EXPECT_EQ(family.get(mdl.get(), 2, nullptr, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
        EXPECT_EQ(family.get(mdl.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    }
}

TEST(CApiTest, IndexList)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model mdl(env);
        add_items(mdl, 13);
        ASSERT_EQ(name(family, mdl, {0, 4, 10, 11}, {"a", "e", "k", "k"}), BIYOS_OK);
        const char *names[] = {"e", "a", "e"};
        std::array<BIYOS_index, 3> found{-1, -1, -1};
        ASSERT_EQ(family.find(mdl.get(), 3, names, found.data()), BIYOS_OK);
        EXPECT_EQ(found, (std::array<BIYOS_index, 3>{4, 0, 4}));
        EXPECT_EQ(family.find(mdl.get(), 0, nullptr, nullptr), BIYOS_OK);
        EXPECT_EQ(family.find(mdl.get(), 3, names, nullptr), BIYOS_OK);
        std::array<BIYOS_index, 3> kept{-7, -7, -7};
        const char *unknown[] = {"a", "z", "k"};
        EXPECT_EQ(family.find(mdl.get(), 3, unknown, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: " + family.noun + " 'z': unknown");
        const char *repeated[] = {"a", "k", "z"};
        EXPECT_EQ(family.find(mdl.get(), 3, repeated, kept.data()), BIYOS_ERR_BADARG);
        const std::string held = "names 2 " + family.noun + "s";
        EXPECT_EQ(env.message(), "invalid argument: " + family.noun + " 'k': " + held);
        EXPECT_EQ(kept, (std::array<BIYOS_index, 3>{-7, -7, -7}));
        const char *missing[] = {"a", nullptr};
        EXPECT_EQ(family.find(mdl.get(), 2, missing, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': entry 1 is null");
        EXPECT_EQ(family.find(mdl.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    }
}

TEST(CApiTest, RefusedNames)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model mdl(env);
        add_items(mdl, 3);
        ASSERT_EQ(name(family, mdl, {0}, {"x"}), BIYOS_OK);
        EXPECT_EQ(name(family, mdl, {2, 1}, {"z", ""}), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 1, "name is empty"));
        EXPECT_EQ(name(family, mdl, {1}, {"y\xed\xa0\x80"}), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 1, "name is not valid UTF-8 at byte 1"));
        EXPECT_EQ(name(family, mdl, {3}, {"y"}), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 3, "unknown"));
        EXPECT_EQ(family.set(mdl.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
        EXPECT_EQ(names_of(family, mdl, 3), std::vector<std::string>({"x", "", ""}));
    }
}

TEST(CApiTest, LockedNames)
{
    const Env env;
    const Model mdl(env);
    add_items(mdl, 1);
    ASSERT_EQ(BIYOS_setmdlname(mdl.get(), "plant"), BIYOS_OK);
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, mdl, {0}, {"x"}), BIYOS_OK);
    }
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = mdl.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env.get(), rename, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
    const BIYOS_status locked = BIYOS_ERR_BADARG;
    const std::array calls{locked, BIYOS_OK, BIYOS_OK, locked};
    std::vector<BIYOS_status> statuses{locked, locked, BIYOS_OK};
    for (std::size_t k = 0; k < kFamilies.size(); ++k)
    {
        for (const BIYOS_status status : calls)
        {
            statuses.push_back(status);
        }
    }
    EXPECT_EQ(attempt.statuses, statuses);
    for (std::size_t k = 0; k < statuses.size(); ++k)
    {
        if (statuses[k] == locked)
        {
            EXPECT_EQ(attempt.messages[k], kLocked) << "attempt " << k;
        }
    }
    EXPECT_EQ(mdl_name(mdl), "plant");
    for (const Calls &family : kFamilies)
    {
        EXPECT_EQ(name_of(family, mdl, 0), "x");
    }
}
