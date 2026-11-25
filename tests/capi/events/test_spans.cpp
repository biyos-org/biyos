#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/timing/clock.hpp"
#include "probes.hpp"

#include "../../io/mps/utils/samples.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{

using biyos::test::written;
using Texts = std::vector<std::string>;
using Models = std::vector<const BIYOS_model *>;

struct Seen
{
    Texts spans;
    Models models;
};

std::string line_of(const BIYOS_event *event)
{
    std::string line = std::string(BIYOS_getevtmod(event)) + "/" + BIYOS_getevttext(event);
    for (BIYOS_index field = 0; field < BIYOS_getnevtfld(event); ++field)
    {
        line += std::string(" ") + BIYOS_getevtfldname(event, field) + "=";
        line += BIYOS_getevtfldval(event, field);
    }
    return line;
}

void keep(const BIYOS_event *event, void *data)
{
    static_cast<Texts *>(data)->push_back(line_of(event));
}

void note(const BIYOS_event *event, void *data)
{
    auto &seen = *static_cast<Seen *>(data);
    seen.spans.push_back(line_of(event));
    seen.models.push_back(BIYOS_getevtmdl(event));
}

template <typename Handle>
using Adder = BIYOS_status (*)(Handle *, BIYOS_hook, void *, BIYOS_int, BIYOS_int, BIYOS_index *);

template <typename Handle> void hook_on(Adder<Handle> add, Handle *handle, Seen &seen)
{
    BIYOS_index id = -1;
    EXPECT_EQ(add(handle, note, &seen, BIYOS_EVTKND_SPAN, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
}

void hook(BIYOS_model *model, Seen &seen)
{
    hook_on(BIYOS_addmdlhook, model, seen);
}

void hook(BIYOS_env *env, Seen &seen)
{
    hook_on(BIYOS_addenvhook, env, seen);
}

BIYOS_env fake_env()
{
    return BIYOS_env{.core = biyos::Env(std::make_unique<biyos::FakeClock>())};
}

Texts span_of(const std::string &name)
{
    return Texts{
        name + " edge=begin wall=0 cpu=0",
        name + " edge=end wall=0 cpu=0",
    };
}

class Recorder
{
  public:
    Recorder()
    {
        BIYOS_index hook = -1;
        const BIYOS_status added =
            BIYOS_addenvhook(&env, keep, &spans, BIYOS_EVTKND_SPAN, BIYOS_EVTLVL_DBG, &hook);
        EXPECT_EQ(added, BIYOS_OK);
        EXPECT_EQ(BIYOS_newmdl(&env, &model), BIYOS_OK);
    }
    ~Recorder()
    {
        BIYOS_freemdl(other);
        BIYOS_freemdl(model);
    }
    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    void watch()
    {
        EXPECT_EQ(BIYOS_newmdl(&env, &other), BIYOS_OK);
        hook(model, own);
        hook(other, stray);
    }

    BIYOS_env env = fake_env();
    BIYOS_model *model = nullptr;
    BIYOS_model *other = nullptr;
    Texts spans;
    Seen own;
    Seen stray;
};

Texts repeated(const Texts &span, int times)
{
    Texts all;
    for (int k = 0; k < times; ++k)
    {
        all.insert(all.end(), span.begin(), span.end());
    }
    return all;
}

void expect_heard(const Recorder &recorder, const Texts &expected)
{
    EXPECT_EQ(recorder.spans, expected);
    EXPECT_EQ(recorder.own.spans, expected);
    EXPECT_EQ(recorder.own.models, Models(expected.size(), recorder.model));
    EXPECT_TRUE(recorder.stray.spans.empty());
}

void expect_unhooked(const Recorder &recorder, const Seen &shared)
{
    EXPECT_TRUE(recorder.own.spans.empty());
    EXPECT_TRUE(recorder.stray.spans.empty());
    EXPECT_EQ(shared.models, Models(shared.spans.size(), nullptr));
}

}

TEST(CApiTest, CopySpan)
{
    Recorder recorder;
    BIYOS_model *copy = nullptr;
    ASSERT_EQ(BIYOS_copymdl(recorder.model, &recorder.env, &copy), BIYOS_OK);
    EXPECT_EQ(recorder.spans, span_of("model/copy"));
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
    recorder.spans.clear();
    EXPECT_EQ(BIYOS_copymdl(recorder.model, &recorder.env, nullptr), BIYOS_ERR_BADARG);
    EXPECT_TRUE(recorder.spans.empty());
}

TEST(CApiTest, DeleteSpan)
{
    Recorder recorder;
    ASSERT_EQ(BIYOS_addvar(recorder.model, 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index first = 0;
    ASSERT_EQ(BIYOS_delvarsp(recorder.model, 1, &first, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_delvarrg(recorder.model, 0, 1, nullptr), BIYOS_OK);
    const Texts span = span_of("model/delete");
    Texts expected = span;
    expected.insert(expected.end(), span.begin(), span.end());
    EXPECT_EQ(recorder.spans, expected);
    recorder.spans.clear();
    EXPECT_EQ(BIYOS_delvarsp(recorder.model, 1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delvarrg(recorder.model, 0, 9, nullptr), BIYOS_ERR_BADARG);
    EXPECT_TRUE(recorder.spans.empty());
}

TEST(CApiTest, ColumnSpan)
{
    Recorder recorder;
    BIYOS_model *model = recorder.model;
    ASSERT_EQ(BIYOS_addvar(model, 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index both[] = {1, 0};
    BIYOS_offset nnz = -1;
    ASSERT_EQ(BIYOS_getnclintermsp(model, 2, both, &nnz), BIYOS_OK);
    ASSERT_EQ(BIYOS_getnclintermrg(model, 0, 2, &nnz), BIYOS_OK);
    ASSERT_EQ(BIYOS_getclintermsp(model, 2, both, 0, &nnz, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_getclintermrg(model, 0, 2, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    const Texts span = span_of("model/columns");
    Texts expected;
    for (int call = 0; call < 4; ++call)
    {
        expected.insert(expected.end(), span.begin(), span.end());
    }
    EXPECT_EQ(recorder.spans, expected);
    recorder.spans.clear();
    EXPECT_EQ(BIYOS_getnclintermsp(model, 1, nullptr, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnclintermrg(model, 0, 3, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getclintermsp(model, 1, nullptr, 0, &nnz, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_TRUE(recorder.spans.empty());
}

TEST(CApiTest, CopyHooked)
{
    Recorder recorder;
    recorder.watch();
    BIYOS_model *copy = nullptr;
    ASSERT_EQ(BIYOS_copymdl(recorder.model, &recorder.env, &copy), BIYOS_OK);
    expect_heard(recorder, span_of("model/copy"));
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
}

TEST(CApiTest, CopyAcross)
{
    Recorder recorder;
    recorder.watch();
    BIYOS_env target = fake_env();
    Seen elsewhere;
    hook(&target, elsewhere);
    BIYOS_model *copy = nullptr;
    ASSERT_EQ(BIYOS_copymdl(recorder.model, &target, &copy), BIYOS_OK);
    expect_heard(recorder, span_of("model/copy"));
    EXPECT_TRUE(elsewhere.spans.empty());
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
}

TEST(CApiTest, DeleteHooked)
{
    Recorder recorder;
    ASSERT_EQ(BIYOS_addvar(recorder.model, 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    recorder.watch();
    const BIYOS_index first = 0;
    ASSERT_EQ(BIYOS_delvarsp(recorder.model, 1, &first, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_delvarrg(recorder.model, 0, 1, nullptr), BIYOS_OK);
    expect_heard(recorder, repeated(span_of("model/delete"), 2));
}

TEST(CApiTest, ColumnHooked)
{
    Recorder recorder;
    BIYOS_model *model = recorder.model;
    ASSERT_EQ(BIYOS_addvar(model, 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    recorder.watch();
    const BIYOS_index both[] = {1, 0};
    BIYOS_offset nnz = -1;
    ASSERT_EQ(BIYOS_getnclintermsp(model, 2, both, &nnz), BIYOS_OK);
    ASSERT_EQ(BIYOS_getnclintermrg(model, 0, 2, &nnz), BIYOS_OK);
    ASSERT_EQ(BIYOS_getclintermsp(model, 2, both, 0, &nnz, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_getclintermrg(model, 0, 2, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    expect_heard(recorder, repeated(span_of("model/columns"), 4));
}

TEST(CApiTest, WriteHooked)
{
    Recorder recorder;
    recorder.watch();
    const std::string path = written("biyos_hooked.mps", "");
    ASSERT_EQ(BIYOS_writemdl(recorder.model, path.c_str(), BIYOS_FILEFMT_MPS), BIYOS_OK);
    expect_heard(recorder, span_of("io/write"));
}

TEST(CApiTest, ReadUnhooked)
{
    Recorder recorder;
    recorder.watch();
    Seen shared;
    hook(&recorder.env, shared);
    const std::string path = written("biyos_unhooked.mps", biyos::test::kLpMpsFile);
    BIYOS_model *read = nullptr;
    ASSERT_EQ(BIYOS_readmdl(&recorder.env, path.c_str(), BIYOS_FILEFMT_MPS, &read), BIYOS_OK);
    EXPECT_EQ(shared.spans, span_of("io/read"));
    expect_unhooked(recorder, shared);
    EXPECT_EQ(BIYOS_freemdl(read), BIYOS_OK);
}

TEST(CApiTest, NewUnhooked)
{
    Recorder recorder;
    recorder.watch();
    Seen shared;
    BIYOS_index id = -1;
    ASSERT_EQ(
        BIYOS_addenvhook(&recorder.env, note, &shared, BIYOS_EVTKND_MSG, BIYOS_EVTLVL_ERR, &id),
        BIYOS_OK);
    EXPECT_EQ(BIYOS_newmdl(&recorder.env, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(shared.models, Models{nullptr});
    expect_unhooked(recorder, shared);
}
