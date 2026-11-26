#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;
using Texts = std::vector<std::string>;
using Models = std::vector<const BIYOS_model *>;

const std::string kNamed = "model name 'a' against none";
const std::string kRefused = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";

struct Heard
{
    Texts lines;
    Models models;
};

struct Inside
{
    BIYOS_model *a = nullptr;
    BIYOS_model *b = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<BIYOS_int> answers;
};

std::string line_of(BIYOS_int level, const std::string &text, BIYOS_index fields)
{
    return std::to_string(level) + " " + text + " fields=" + std::to_string(fields);
}

void note(const BIYOS_event *event, void *data)
{
    Heard &heard = *static_cast<Heard *>(data);
    const std::string text = std::string(BIYOS_getevtmod(event)) + ": " + BIYOS_getevttext(event);
    heard.lines.push_back(line_of(BIYOS_getevtlvl(event), text, BIYOS_getnevtfld(event)));
    heard.models.push_back(BIYOS_getevtmdl(event));
}

void compare_inside(const BIYOS_event *, void *data)
{
    Inside &inside = *static_cast<Inside *>(data);
    BIYOS_int same = -1;
    inside.statuses.push_back(BIYOS_cmpmdl(inside.a, inside.b, &same));
    inside.answers.push_back(same);
}

void rename_inside(const BIYOS_event *, void *data)
{
    Inside &inside = *static_cast<Inside *>(data);
    inside.statuses.push_back(BIYOS_setmdlname(inside.b, "b"));
}

template <typename Handle>
using Adder = BIYOS_status (*)(Handle *, BIYOS_hook, void *, BIYOS_int, BIYOS_int, BIYOS_index *);

template <typename Handle>
void listen(Adder<Handle> add, Handle *handle, BIYOS_hook hook, void *data, BIYOS_int level)
{
    BIYOS_index id = -1;
    EXPECT_EQ(add(handle, hook, data, BIYOS_EVTKND_MSG, level, &id), BIYOS_OK);
}

void listen(const Env &env, Heard &heard)
{
    listen(BIYOS_addenvhook, env.get(), note, &heard, BIYOS_EVTLVL_DBG);
}

void listen(const Model &model, Heard &heard)
{
    listen(BIYOS_addmdlhook, model.get(), note, &heard, BIYOS_EVTLVL_DBG);
}

void listen(const Env &env, BIYOS_hook hook, Inside &inside, BIYOS_int level)
{
    listen(BIYOS_addenvhook, env.get(), hook, &inside, level);
}

BIYOS_int compared(const Model &a, const Model &b)
{
    BIYOS_int same = -1;
    EXPECT_EQ(BIYOS_cmpmdl(a.get(), b.get(), &same), BIYOS_OK);
    return same;
}

std::string info(const std::string &text)
{
    return line_of(BIYOS_EVTLVL_INFO, "model: " + text, 0);
}

void add_sample(const Model &model)
{
    const double lower[] = {0.0, -1.0};
    const double upper[] = {1.0, BIYOS_INF};
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, nullptr, lower, upper, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 1, nullptr), BIYOS_OK);
}

}

TEST(CApiTest, EqualCompare)
{
    const Env env;
    Heard heard;
    listen(env, heard);
    const Model empty(env);
    const Model blank(env);
    EXPECT_EQ(compared(empty, blank), 1);
    EXPECT_EQ(compared(empty, empty), 1);
    add_sample(empty);
    const Model copy(env, empty);
    EXPECT_EQ(compared(empty, copy), 1);
    EXPECT_EQ(compared(copy, empty), 1);
    EXPECT_EQ(heard.lines, Texts{});
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, UnequalCompare)
{
    const Env env;
    const Model a(env);
    const Model b(env);
    Heard on_env;
    Heard on_a;
    Heard on_b;
    listen(env, on_env);
    listen(a, on_a);
    listen(b, on_b);
    add_sample(a);
    add_sample(b);
    const double bound = 2.0;
    const BIYOS_index first = 0;
    ASSERT_EQ(BIYOS_setvarbndsp(b.get(), 1, &first, nullptr, &bound), BIYOS_OK);
    EXPECT_EQ(compared(a, b), 0);
    const Texts upper{info("variable 0: upper bound 1 against 2")};
    EXPECT_EQ(on_env.lines, upper);
    EXPECT_EQ(on_env.models, Models{a.get()});
    EXPECT_EQ(on_a.lines, upper);
    EXPECT_EQ(on_b.lines, Texts{});
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, OtherCompare)
{
    const Env first;
    const Env second;
    const Model a(first);
    const Model b(second);
    Heard on_first;
    Heard on_second;
    listen(first, on_first);
    listen(second, on_second);
    ASSERT_EQ(BIYOS_setmdlname(a.get(), "a"), BIYOS_OK);
    EXPECT_EQ(compared(a, b), 0);
    EXPECT_EQ(on_first.lines, Texts{info(kNamed)});
    EXPECT_EQ(on_second.lines, Texts{});
    EXPECT_EQ(compared(b, a), 0);
    EXPECT_EQ(on_second.lines, Texts{info("model name none against 'a'")});
    EXPECT_EQ(on_first.lines, Texts{info(kNamed)});
}

TEST(CApiTest, NullCompare)
{
    const Env env;
    const Model model(env);
    BIYOS_int same = -1;
    EXPECT_EQ(BIYOS_cmpmdl(nullptr, model.get(), &same), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(BIYOS_cmpmdl(model.get(), nullptr, &same), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'b': is null");
    EXPECT_EQ(BIYOS_cmpmdl(model.get(), model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'same': is null");
    EXPECT_EQ(same, -1);
}

TEST(CApiTest, HookCompare)
{
    const Env env;
    const Model a(env);
    const Model b(env);
    ASSERT_EQ(BIYOS_setmdlname(a.get(), "a"), BIYOS_OK);
    Inside inside{.a = a.get(), .b = b.get(), .statuses = {}, .answers = {}};
    Heard heard;
    listen(env, heard);
    listen(env, compare_inside, inside, BIYOS_EVTLVL_ERR);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(inside.statuses, std::vector<BIYOS_status>{BIYOS_OK});
    EXPECT_EQ(inside.answers, std::vector<BIYOS_int>{0});
    const std::string refused = line_of(BIYOS_EVTLVL_ERR, "capi: " + kRefused, 1);
    EXPECT_EQ(heard.lines, (Texts{refused, info(kNamed)}));
}

TEST(CApiTest, OtherHookCompare)
{
    const Env first;
    const Env second;
    const Model a(first);
    const Model b(second);
    ASSERT_EQ(BIYOS_setmdlname(a.get(), "a"), BIYOS_OK);
    Inside inside{.a = a.get(), .b = b.get(), .statuses = {}, .answers = {}};
    Heard heard;
    listen(first, heard);
    listen(second, compare_inside, inside, BIYOS_EVTLVL_ERR);
    EXPECT_EQ(BIYOS_setenvintopt(second.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(inside.statuses, std::vector<BIYOS_status>{BIYOS_OK});
    EXPECT_EQ(inside.answers, std::vector<BIYOS_int>{0});
    EXPECT_EQ(heard.lines, Texts{info(kNamed)});
}

TEST(CApiTest, ChangeCompare)
{
    const Env first;
    const Env second;
    const Model a(first);
    const Model b(second);
    ASSERT_EQ(BIYOS_setmdlname(a.get(), "a"), BIYOS_OK);
    Inside inside{.a = a.get(), .b = b.get(), .statuses = {}, .answers = {}};
    listen(first, rename_inside, inside, BIYOS_EVTLVL_INFO);
    EXPECT_EQ(compared(a, b), 0);
    EXPECT_EQ(inside.statuses, std::vector<BIYOS_status>{BIYOS_ERR_BUSY});
    EXPECT_EQ(second.message(), "busy: model: is in a call on another thread");
    EXPECT_EQ(compared(b, a), 0);
}
