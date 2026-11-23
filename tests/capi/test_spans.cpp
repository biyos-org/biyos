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
