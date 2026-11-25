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
    BIYOS_status (*remove)(BIYOS_model *, BIYOS_index, const BIYOS_index *);
};

const std::array<Calls, 8> kFamilies{{
    {
        .noun = "variable",
        .set = BIYOS_setvarnamesp,
        .get = BIYOS_getvarnamesp,
        .find = BIYOS_getvaridx,
        .remove = BIYOS_delvarnamesp,
    },
    {
        .noun = "function",
        .set = BIYOS_setfunnamesp,
        .get = BIYOS_getfunnamesp,
        .find = BIYOS_getfunidx,
        .remove = BIYOS_delfunnamesp,
    },
    {
        .noun = "constraint",
        .set = BIYOS_setconnamesp,
        .get = BIYOS_getconnamesp,
        .find = BIYOS_getconidx,
        .remove = BIYOS_delconnamesp,
    },
    {
        .noun = "objective",
        .set = BIYOS_setobjnamesp,
        .get = BIYOS_getobjnamesp,
        .find = BIYOS_getobjidx,
        .remove = BIYOS_delobjnamesp,
    },
    {
        .noun = "indicator",
        .set = BIYOS_setindnamesp,
        .get = BIYOS_getindnamesp,
        .find = BIYOS_getindidx,
        .remove = BIYOS_delindnamesp,
    },
    {
        .noun = "set",
        .set = BIYOS_setsosnamesp,
        .get = BIYOS_getsosnamesp,
        .find = BIYOS_getsosidx,
        .remove = BIYOS_delsosnamesp,
    },
    {
        .noun = "cone",
        .set = BIYOS_setconenamesp,
        .get = BIYOS_getconenamesp,
        .find = BIYOS_getconeidx,
        .remove = BIYOS_delconenamesp,
    },
    {
        .noun = "complement",
        .set = BIYOS_setcconnamesp,
        .get = BIYOS_getcconnamesp,
        .find = BIYOS_getcconidx,
        .remove = BIYOS_delcconnamesp,
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
                     const BIYOS_model *model,
                     BIYOS_index index,
                     const char **name)
{
    return family.get(model, 1, &index, name);
}

BIYOS_status find_one(const Calls &family,
                      const BIYOS_model *model,
                      const char *name,
                      BIYOS_index *indices)
{
    return family.find(model, 1, &name, indices);
}

void add_cones(BIYOS_model *model, BIYOS_index n)
{
    const std::vector<BIYOS_int> kinds(static_cast<std::size_t>(n), BIYOS_CONEKND_NONNEG);
    const std::vector<BIYOS_offset> counts(static_cast<std::size_t>(n), 1);
    const Indices functions(static_cast<std::size_t>(n), 0);
    const BIYOS_status status = BIYOS_addcone(model,
                                              n,
                                              kinds.data(),
                                              counts.data(),
                                              functions.data(),
                                              nullptr,
                                              nullptr,
                                              nullptr);
    ASSERT_EQ(status, BIYOS_OK);
}

void add_items(const Model &model, BIYOS_index n)
{
    BIYOS_model *held = model.get();
    const Indices functions(static_cast<std::size_t>(n), 0);
    const std::vector<BIYOS_int> kinds(static_cast<std::size_t>(n), BIYOS_VARKND_BIN);
    const std::vector<BIYOS_int> values(static_cast<std::size_t>(n), 1);
    const Indices variables(static_cast<std::size_t>(n), 0);
    const std::vector<BIYOS_int> sos_kinds(static_cast<std::size_t>(n), BIYOS_SOSKND_ONE);
    ASSERT_EQ(BIYOS_addvar(held, n, kinds.data(), nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(held, n, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcon(held, n, functions.data(), nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addobj(held, n, functions.data(), nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    const BIYOS_status status = BIYOS_addind(held,
                                             n,
                                             variables.data(),
                                             values.data(),
                                             functions.data(),
                                             nullptr,
                                             nullptr,
                                             nullptr);
    ASSERT_EQ(status, BIYOS_OK);
    ASSERT_EQ(BIYOS_addsos(held, n, sos_kinds.data(), nullptr, nullptr, nullptr, nullptr),
              BIYOS_OK);
    add_cones(held, n);
    const BIYOS_index *shared = functions.data();
    ASSERT_EQ(BIYOS_addccon(held, n, shared, shared, nullptr, nullptr, nullptr), BIYOS_OK);
}

BIYOS_status name(const Calls &family, const Model &model, const Indices &index, const Texts &names)
{
    const auto n = static_cast<BIYOS_index>(index.size());
    return family.set(model.get(), n, index.data(), names.data());
}

std::string name_of(const Calls &family, const Model &model, BIYOS_index index)
{
    const char *found = nullptr;
    EXPECT_EQ(get_one(family, model.get(), index, &found), BIYOS_OK);
    return found == nullptr ? "null" : found;
}

BIYOS_index index_of(const Calls &family, const Model &model, const char *name)
{
    BIYOS_index index = -1;
    EXPECT_EQ(find_one(family, model.get(), name, &index), BIYOS_OK);
    return index;
}

std::vector<std::string> names_of(const Calls &family, const Model &model, BIYOS_index n)
{
    std::vector<std::string> found;
    for (BIYOS_index k = 0; k < n; ++k)
    {
        found.push_back(name_of(family, model, k));
    }
    return found;
}

std::string model_name(const Model &model)
{
    const char *name = nullptr;
    EXPECT_EQ(BIYOS_getmdlname(model.get(), &name), BIYOS_OK);
    return name == nullptr ? "null" : name;
}

std::string refused(const Calls &family, BIYOS_index index, const std::string &reason)
{
    return "invalid argument: " + family.noun + " " + std::to_string(index) + ": " + reason;
}

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(biyos::test::last_message(attempt.env));
}

void rename(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    const BIYOS_index index = 0;
    const char *name = "w";
    const char *found = nullptr;
    BIYOS_index at = -1;
    note(attempt, BIYOS_setmdlname(model, name));
    note(attempt, BIYOS_delmdlname(model));
    note(attempt, BIYOS_getmdlname(model, &found));
    for (const Calls &family : kFamilies)
    {
        note(attempt, family.set(model, 1, &index, &name));
        note(attempt, get_one(family, model, 0, &found));
        note(attempt, find_one(family, model, "x", &at));
        note(attempt, family.remove(model, 1, &index));
    }
}

}

TEST(CApiTest, ModelName)
{
    const Env env;
    const Model model(env);
    EXPECT_EQ(model_name(model), "");
    ASSERT_EQ(BIYOS_setmdlname(model.get(), "plant"), BIYOS_OK);
    EXPECT_EQ(model_name(model), "plant");
    ASSERT_EQ(BIYOS_setmdlname(model.get(), "\xc3\xa9t\xc3\xa9"), BIYOS_OK);
    EXPECT_EQ(model_name(model), "\xc3\xa9t\xc3\xa9");
    EXPECT_EQ(BIYOS_setmdlname(model.get(), ""), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: model: name is empty");
    EXPECT_EQ(BIYOS_setmdlname(model.get(), "ab\xc3"), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: model: name is not valid UTF-8 at byte 2");
    EXPECT_EQ(BIYOS_setmdlname(model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
    EXPECT_EQ(BIYOS_getmdlname(model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
    EXPECT_EQ(model_name(model), "\xc3\xa9t\xc3\xa9");
    const char *name = "kept";
    EXPECT_EQ(BIYOS_setmdlname(nullptr, "plant"), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getmdlname(nullptr, &name), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delmdlname(nullptr), BIYOS_ERR_BADARG);
    EXPECT_STREQ(name, "kept");
    EXPECT_EQ(BIYOS_delmdlname(model.get()), BIYOS_OK);
    EXPECT_EQ(model_name(model), "");
    EXPECT_EQ(BIYOS_delmdlname(model.get()), BIYOS_OK);
    EXPECT_EQ(model_name(model), "");
    EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
}

TEST(CApiTest, FamilyNames)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model model(env);
        add_items(model, 3);
        EXPECT_EQ(names_of(family, model, 3), std::vector<std::string>(3, ""));
        ASSERT_EQ(name(family, model, {2, 0}, {"z", "x"}), BIYOS_OK);
        EXPECT_EQ(names_of(family, model, 3), std::vector<std::string>({"x", "", "z"}));
        EXPECT_EQ(index_of(family, model, "z"), 2);
        EXPECT_EQ(index_of(family, model, "x"), 0);
        ASSERT_EQ(name(family, model, {0, 2, 1, 1}, {"z", "x", "w", "y"}), BIYOS_OK);
        EXPECT_EQ(names_of(family, model, 3), std::vector<std::string>({"z", "y", "x"}));
        EXPECT_EQ(index_of(family, model, "y"), 1);
        EXPECT_EQ(env.message(), "");
        for (const Calls &other : kFamilies)
        {
            if (other.noun != family.noun)
            {
                EXPECT_EQ(names_of(other, model, 3), std::vector<std::string>(3, ""));
            }
        }
    }
}

TEST(CApiTest, SharedName)
{
    const Env env;
    const Model model(env);
    add_items(model, static_cast<BIYOS_index>(kFamilies.size()));
    BIYOS_index index = 0;
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, model, {index}, {"x"}), BIYOS_OK);
        ++index;
    }
    index = 0;
    for (const Calls &family : kFamilies)
    {
        EXPECT_EQ(index_of(family, model, "x"), index);
        ++index;
    }
}

TEST(CApiTest, NameLifetime)
{
    const Env env;
    const Model model(env);
    add_items(model, 2);
    ASSERT_EQ(BIYOS_setmdlname(model.get(), "plant"), BIYOS_OK);
    std::vector<const char *> kept;
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, model, {0}, {"x"}), BIYOS_OK);
        const char *found = nullptr;
        ASSERT_EQ(get_one(family, model.get(), 0, &found), BIYOS_OK);
        kept.push_back(found);
    }
    const char *title = nullptr;
    ASSERT_EQ(BIYOS_getmdlname(model.get(), &title), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvar(model.get(), 499, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    std::vector<std::string> texts;
    Indices index;
    for (BIYOS_index k = 1; k <= 500; ++k)
    {
        index.push_back(k);
        texts.push_back("v" + std::to_string(k));
    }
    Texts names;
    for (const std::string &text : texts)
    {
        names.push_back(text.c_str());
    }
    const Calls &variables = kFamilies[0];
    ASSERT_EQ(name(variables, model, index, names), BIYOS_OK);
    ASSERT_EQ(name(variables, model, {2, 1}, {"v1", "v2"}), BIYOS_OK);
    for (const Calls &family : kFamilies)
    {
        EXPECT_EQ(name(family, model, {0}, {""}), BIYOS_ERR_BADARG);
        ASSERT_EQ(name(family, model, {1}, {"y"}), BIYOS_OK);
    }
    EXPECT_EQ(name(variables, model, {0, 1}, {"v3", ""}), BIYOS_ERR_BADARG);
    EXPECT_EQ(kFamilies[0].remove(model.get(), 1, &index[9]), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlname(model.get(), ""), BIYOS_ERR_BADARG);
    BIYOS_model *copy = nullptr;
    ASSERT_EQ(BIYOS_copymdl(model.get(), env.get(), &copy), BIYOS_OK);
    const char *copied = nullptr;
    const BIYOS_index zero = 0;
    ASSERT_EQ(BIYOS_getvarnamesp(copy, 1, &zero, &copied), BIYOS_OK);
    EXPECT_NE(copied, kept[0]);
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
    EXPECT_EQ(index_of(variables, model, "v500"), 500);
    EXPECT_STREQ(title, "plant");
    for (std::size_t k = 0; k < kFamilies.size(); ++k)
    {
        EXPECT_STREQ(kept[k], "x");
        const char *again = nullptr;
        ASSERT_EQ(get_one(kFamilies[k], model.get(), 0, &again), BIYOS_OK);
        EXPECT_EQ(again, kept[k]);
    }
}

TEST(CApiTest, CopiedName)
{
    const Env env;
    BIYOS_model *source = nullptr;
    ASSERT_EQ(BIYOS_newmdl(env.get(), &source), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvar(source, 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index index = 0;
    const char *name = "x";
    ASSERT_EQ(BIYOS_setvarnamesp(source, 1, &index, &name), BIYOS_OK);
    BIYOS_model *copy = nullptr;
    ASSERT_EQ(BIYOS_copymdl(source, env.get(), &copy), BIYOS_OK);
    const char *found = nullptr;
    ASSERT_EQ(BIYOS_getvarnamesp(copy, 1, &index, &found), BIYOS_OK);
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
    const Model model(env);
    add_items(model, 2);
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, model, {0}, {family.noun.c_str()}), BIYOS_OK);
    }
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const std::string unknown = "invalid argument: " + family.noun + " '";
        const char *other = family.noun == "variable" ? "function" : "variable";
        BIYOS_index index = -7;
        EXPECT_EQ(find_one(family, model.get(), "w", &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "w': unknown");
        EXPECT_EQ(find_one(family, model.get(), "", &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "': unknown");
        EXPECT_EQ(find_one(family, model.get(), other, &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + other + "': unknown");
        EXPECT_EQ(find_one(family, model.get(), "\xff", &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "\xff': unknown");
        EXPECT_EQ(family.find(model.get(), 1, nullptr, &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
        EXPECT_EQ(find_one(family, model.get(), nullptr, &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': entry 0 is null");
        EXPECT_EQ(find_one(family, model.get(), "w", nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + "w': unknown");
        EXPECT_EQ(find_one(family, model.get(), family.noun.c_str(), nullptr), BIYOS_OK);
        EXPECT_EQ(find_one(family, nullptr, family.noun.c_str(), &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(index, -7);
        ASSERT_EQ(name(family, model, {0}, {"renamed"}), BIYOS_OK);
        EXPECT_EQ(find_one(family, model.get(), family.noun.c_str(), &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), unknown + family.noun + "': unknown");
        EXPECT_EQ(index, -7);
    }
}

TEST(CApiTest, NullNames)
{
    const Env env;
    const Model model(env);
    add_items(model, 3);
    BIYOS_model *held = model.get();
    const BIYOS_index indices[] = {0, 1};
    const char *names[] = {"x", nullptr};
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        EXPECT_EQ(family.set(held, 0, nullptr, nullptr), BIYOS_OK);
        EXPECT_EQ(family.set(held, 2, nullptr, names), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
        EXPECT_EQ(family.set(held, 2, indices, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': is null");
        EXPECT_EQ(family.set(held, 2, indices, names), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': entry 1 is null");
        EXPECT_EQ(get_one(family, held, 0, nullptr), BIYOS_OK);
        EXPECT_EQ(get_one(family, held, 3, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 3, "unknown"));
        EXPECT_EQ(family.set(nullptr, 1, indices, names), BIYOS_ERR_BADARG);
        EXPECT_EQ(name_of(family, model, 0), "");
    }
}

TEST(CApiTest, RepeatName)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model model(env);
        add_items(model, 3);
        ASSERT_EQ(name(family, model, {0, 1, 2}, {"c", "c", "c"}), BIYOS_OK);
        EXPECT_EQ(names_of(family, model, 3), std::vector<std::string>(3, "c"));
        BIYOS_index index = -7;
        const std::string repeated = "invalid argument: " + family.noun + " 'c': names ";
        EXPECT_EQ(find_one(family, model.get(), "c", &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), repeated + "3 " + family.noun + "s");
        ASSERT_EQ(name(family, model, {0}, {"d"}), BIYOS_OK);
        EXPECT_EQ(find_one(family, model.get(), "c", &index), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), repeated + "2 " + family.noun + "s");
        EXPECT_EQ(index, -7);
        const BIYOS_index second = 1;
        ASSERT_EQ(family.remove(model.get(), 1, &second), BIYOS_OK);
        EXPECT_EQ(index_of(family, model, "c"), 2);
        EXPECT_EQ(index_of(family, model, "d"), 0);
    }
}

TEST(CApiTest, DeletedName)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model model(env);
        add_items(model, 3);
        ASSERT_EQ(name(family, model, {0, 1, 2}, {"a", "b", "c"}), BIYOS_OK);
        const BIYOS_index indices[] = {1, 0, 1};
        EXPECT_EQ(family.remove(model.get(), 3, indices), BIYOS_OK);
        EXPECT_EQ(names_of(family, model, 3), std::vector<std::string>({"", "", "c"}));
        EXPECT_EQ(family.remove(model.get(), 0, nullptr), BIYOS_OK);
        EXPECT_EQ(family.remove(model.get(), 1, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
        EXPECT_EQ(family.remove(model.get(), -1, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
        const BIYOS_index outside[] = {2, 3};
        EXPECT_EQ(family.remove(model.get(), 2, outside), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 3, "unknown"));
        EXPECT_EQ(family.remove(nullptr, 1, indices), BIYOS_ERR_BADARG);
        EXPECT_EQ(names_of(family, model, 3), std::vector<std::string>({"", "", "c"}));
        BIYOS_index found = -7;
        EXPECT_EQ(find_one(family, model.get(), "a", &found), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: " + family.noun + " 'a': unknown");
        ASSERT_EQ(name(family, model, {1}, {"a"}), BIYOS_OK);
        EXPECT_EQ(index_of(family, model, "a"), 1);
    }
}

TEST(CApiTest, NameList)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model model(env);
        add_items(model, 13);
        ASSERT_EQ(name(family, model, {0, 4, 10}, {"a", "e", "k"}), BIYOS_OK);
        const BIYOS_index indices[] = {0, 4, 10, 12, 4};
        std::array<const char *, 5> found{};
        ASSERT_EQ(family.get(model.get(), 5, indices, found.data()), BIYOS_OK);
        const std::vector<std::string> read(found.begin(), found.end());
        EXPECT_EQ(read, std::vector<std::string>({"a", "e", "k", "", "e"}));
        EXPECT_EQ(found[1], found[4]);
        EXPECT_EQ(family.get(model.get(), 0, nullptr, nullptr), BIYOS_OK);
        EXPECT_EQ(family.get(model.get(), 5, indices, nullptr), BIYOS_OK);
        const BIYOS_index outside[] = {0, 13};
        std::array<const char *, 2> kept{"kept", "kept"};
        EXPECT_EQ(family.get(model.get(), 2, outside, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 13, "unknown"));
        EXPECT_STREQ(kept[0], "kept");
        EXPECT_EQ(family.get(model.get(), 2, nullptr, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
        EXPECT_EQ(family.get(model.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    }
}

TEST(CApiTest, IndexList)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model model(env);
        add_items(model, 13);
        ASSERT_EQ(name(family, model, {0, 4, 10, 11}, {"a", "e", "k", "k"}), BIYOS_OK);
        const char *names[] = {"e", "a", "e"};
        std::array<BIYOS_index, 3> found{-1, -1, -1};
        ASSERT_EQ(family.find(model.get(), 3, names, found.data()), BIYOS_OK);
        EXPECT_EQ(found, (std::array<BIYOS_index, 3>{4, 0, 4}));
        EXPECT_EQ(family.find(model.get(), 0, nullptr, nullptr), BIYOS_OK);
        EXPECT_EQ(family.find(model.get(), 3, names, nullptr), BIYOS_OK);
        std::array<BIYOS_index, 3> kept{-7, -7, -7};
        const char *unknown[] = {"a", "z", "k"};
        EXPECT_EQ(family.find(model.get(), 3, unknown, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: " + family.noun + " 'z': unknown");
        const char *repeated[] = {"a", "k", "z"};
        EXPECT_EQ(family.find(model.get(), 3, repeated, kept.data()), BIYOS_ERR_BADARG);
        const std::string held = "names 2 " + family.noun + "s";
        EXPECT_EQ(env.message(), "invalid argument: " + family.noun + " 'k': " + held);
        EXPECT_EQ(kept, (std::array<BIYOS_index, 3>{-7, -7, -7}));
        const char *missing[] = {"a", nullptr};
        EXPECT_EQ(family.find(model.get(), 2, missing, kept.data()), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'name': entry 1 is null");
        EXPECT_EQ(family.find(model.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    }
}

TEST(CApiTest, RefusedNames)
{
    for (const Calls &family : kFamilies)
    {
        SCOPED_TRACE(family.noun);
        const Env env;
        const Model model(env);
        add_items(model, 3);
        ASSERT_EQ(name(family, model, {0}, {"x"}), BIYOS_OK);
        EXPECT_EQ(name(family, model, {2, 1}, {"z", ""}), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 1, "name is empty"));
        EXPECT_EQ(name(family, model, {1}, {"y\xed\xa0\x80"}), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 1, "name is not valid UTF-8 at byte 1"));
        EXPECT_EQ(name(family, model, {3}, {"y"}), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), refused(family, 3, "unknown"));
        EXPECT_EQ(family.set(model.get(), -1, nullptr, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
        EXPECT_EQ(names_of(family, model, 3), std::vector<std::string>({"x", "", ""}));
    }
}

TEST(CApiTest, LockedNames)
{
    const Env env;
    const Model model(env);
    add_items(model, 1);
    ASSERT_EQ(BIYOS_setmdlname(model.get(), "plant"), BIYOS_OK);
    for (const Calls &family : kFamilies)
    {
        ASSERT_EQ(name(family, model, {0}, {"x"}), BIYOS_OK);
    }
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), rename, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
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
    EXPECT_EQ(model_name(model), "plant");
    for (const Calls &family : kFamilies)
    {
        EXPECT_EQ(name_of(family, model, 0), "x");
    }
}
