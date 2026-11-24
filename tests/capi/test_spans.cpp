#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "core/clock.hpp"
#include "core/env.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{

using Texts = std::vector<std::string>;

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
    ~Recorder() { BIYOS_freemdl(model); }
    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    BIYOS_env env{.core = biyos::Env(std::make_unique<biyos::FakeClock>())};
    BIYOS_model *model = nullptr;
    Texts spans;
};

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
