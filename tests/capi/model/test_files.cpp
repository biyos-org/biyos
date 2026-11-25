#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/timing/clock.hpp"
#include "handles.hpp"
#include "probes.hpp"

#include "../../io/mps/utils/samples.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::mark;
using biyos::test::Model;
using biyos::test::written;

struct Read
{
    BIYOS_status status;
    BIYOS_model *model;
};

Read read(const Env &env, const char *path, BIYOS_int fmt)
{
    const Model kept(env);
    BIYOS_model *model = kept.get();
    const BIYOS_status status = BIYOS_readmdl(env.get(), path, fmt, &model);
    return Read{.status = status, .model = model};
}

BIYOS_index count_of(BIYOS_status (*count)(const BIYOS_model *, BIYOS_index *),
                     const BIYOS_model *model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(count(model, &n), BIYOS_OK);
    return n;
}

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
    static_cast<std::vector<std::string> *>(data)->push_back(line_of(event));
}

BIYOS_status listen(BIYOS_model *model, BIYOS_hook hook, void *data)
{
    BIYOS_index id = -1;
    return BIYOS_addmdlhook(model, hook, data, BIYOS_EVTKND_MSG, BIYOS_EVTLVL_WARN, &id);
}

}

TEST(CApiTest, ReadFile)
{
    const Env env;
    const std::string path = written("biyos_lp.mps", biyos::test::kLpMpsFile);
    const Read made = read(env, path.c_str(), BIYOS_FILEFMT_MPS);
    ASSERT_EQ(made.status, BIYOS_OK);
    EXPECT_EQ(count_of(BIYOS_getnvar, made.model), 2);
    EXPECT_EQ(count_of(BIYOS_getncon, made.model), 3);
    EXPECT_EQ(count_of(BIYOS_getnobj, made.model), 1);
    const char *name = nullptr;
    EXPECT_EQ(BIYOS_getmdlname(made.model, &name), BIYOS_OK);
    EXPECT_STREQ(name, "small");
    EXPECT_EQ(BIYOS_freemdl(made.model), BIYOS_OK);
}

TEST(CApiTest, MissingFile)
{
    const Env env;
    const std::string path = testing::TempDir() + "biyos_absent/t.mps";
    const Read made = read(env, path.c_str(), BIYOS_FILEFMT_MPS);
    EXPECT_EQ(made.status, BIYOS_ERR_FILEIO);
    EXPECT_EQ(made.model, nullptr);
    EXPECT_EQ(env.message(), "file access: file '" + path + "': cannot be opened");
}

TEST(CApiTest, BadFile)
{
    const Env env;
    const std::string path = testing::TempDir() + "biyos_bad.mps";
    std::ofstream(path) << "NAME t\nROWS\n N f\nCOLUMNS\n x g 1\nENDATA\n";
    const Read made = read(env, path.c_str(), BIYOS_FILEFMT_MPS);
    EXPECT_EQ(made.status, BIYOS_ERR_BADFILE);
    EXPECT_EQ(made.model, nullptr);
    EXPECT_EQ(env.message(), "bad file: file '" + path + "' line 5: unknown row 'g'");
}

TEST(CApiTest, ReadRefused)
{
    const Env env;
    const std::string path = written("biyos_lp.mps", biyos::test::kLpMpsFile);
    EXPECT_EQ(read(env, nullptr, BIYOS_FILEFMT_MPS).status, BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'path': is null");
    EXPECT_EQ(read(env, path.c_str(), 7).status, BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: format 7: unknown");
    EXPECT_EQ(BIYOS_readmdl(env.get(), path.c_str(), BIYOS_FILEFMT_MPS, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_readmdl(nullptr, path.c_str(), BIYOS_FILEFMT_MPS, nullptr), BIYOS_ERR_BADARG);
}

TEST(CApiTest, ReadSpan)
{
    BIYOS_env env{.core = biyos::Env(std::make_unique<biyos::FakeClock>())};
    std::vector<std::string> spans;
    BIYOS_index hook = -1;
    ASSERT_EQ(BIYOS_addenvhook(&env, keep, &spans, BIYOS_EVTKND_SPAN, BIYOS_EVTLVL_DBG, &hook),
              BIYOS_OK);
    const std::string path = written("biyos_lp.mps", biyos::test::kLpMpsFile);
    BIYOS_model *model = nullptr;
    ASSERT_EQ(BIYOS_readmdl(&env, path.c_str(), BIYOS_FILEFMT_MPS, &model), BIYOS_OK);
    const std::vector<std::string> expected{
        "io/read edge=begin wall=0 cpu=0",
        "io/read edge=end wall=0 cpu=0",
    };
    EXPECT_EQ(spans, expected);
    EXPECT_EQ(BIYOS_freemdl(model), BIYOS_OK);
    spans.clear();
    EXPECT_EQ(BIYOS_readmdl(&env, nullptr, BIYOS_FILEFMT_MPS, &model), BIYOS_ERR_BADARG);
    EXPECT_TRUE(spans.empty());
}

TEST(CApiTest, WriteFile)
{
    const Env env;
    const std::string source = written("biyos_lp.mps", biyos::test::kLpMpsFile);
    const Read made = read(env, source.c_str(), BIYOS_FILEFMT_MPS);
    ASSERT_EQ(made.status, BIYOS_OK);
    const std::string path = testing::TempDir() + "biyos_written.mps";
    EXPECT_EQ(BIYOS_writemdl(made.model, path.c_str(), BIYOS_FILEFMT_MPS), BIYOS_OK);
    const Read back = read(env, path.c_str(), BIYOS_FILEFMT_MPS);
    ASSERT_EQ(back.status, BIYOS_OK);
    EXPECT_EQ(count_of(BIYOS_getnvar, back.model), 2);
    EXPECT_EQ(count_of(BIYOS_getncon, back.model), 3);
    EXPECT_EQ(count_of(BIYOS_getnobj, back.model), 1);
    EXPECT_EQ(BIYOS_freemdl(back.model), BIYOS_OK);
    EXPECT_EQ(BIYOS_freemdl(made.model), BIYOS_OK);
}

TEST(CApiTest, WriteRefused)
{
    const Env env;
    const Model model(env);
    const std::string path = testing::TempDir() + "absent/biyos.mps";
    EXPECT_EQ(BIYOS_writemdl(model.get(), path.c_str(), BIYOS_FILEFMT_MPS), BIYOS_ERR_FILEIO);
    EXPECT_EQ(env.message(), "file access: file '" + path + "': cannot be opened");
    const std::string folder = testing::TempDir();
    EXPECT_EQ(BIYOS_writemdl(model.get(), folder.c_str(), BIYOS_FILEFMT_MPS), BIYOS_ERR_FILEIO);
    EXPECT_EQ(env.message(), "file access: file '" + folder + "': is a directory");
    EXPECT_EQ(BIYOS_writemdl(model.get(), nullptr, BIYOS_FILEFMT_MPS), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'path': is null");
    EXPECT_EQ(BIYOS_writemdl(model.get(), path.c_str(), 7), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: format 7: unknown");
    EXPECT_EQ(BIYOS_writemdl(nullptr, path.c_str(), BIYOS_FILEFMT_MPS), BIYOS_ERR_BADARG);
}

TEST(CApiTest, WriteSpan)
{
    BIYOS_env env{.core = biyos::Env(std::make_unique<biyos::FakeClock>())};
    BIYOS_model *model = nullptr;
    ASSERT_EQ(BIYOS_newmdl(&env, &model), BIYOS_OK);
    std::vector<std::string> spans;
    BIYOS_index hook = -1;
    ASSERT_EQ(BIYOS_addenvhook(&env, keep, &spans, BIYOS_EVTKND_SPAN, BIYOS_EVTLVL_DBG, &hook),
              BIYOS_OK);
    const std::string path = testing::TempDir() + "biyos_span.mps";
    ASSERT_EQ(BIYOS_writemdl(model, path.c_str(), BIYOS_FILEFMT_MPS), BIYOS_OK);
    const std::vector<std::string> expected{
        "io/write edge=begin wall=0 cpu=0",
        "io/write edge=end wall=0 cpu=0",
    };
    EXPECT_EQ(spans, expected);
    spans.clear();
    EXPECT_EQ(BIYOS_writemdl(model, nullptr, BIYOS_FILEFMT_MPS), BIYOS_ERR_BADARG);
    EXPECT_TRUE(spans.empty());
    EXPECT_EQ(BIYOS_freemdl(model), BIYOS_OK);
}

TEST(CApiTest, WriteWarned)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_setmdlintopt(model.get(), BIYOS_OPT_SEED, 3), BIYOS_OK);
    std::vector<std::string> heard;
    std::vector<const BIYOS_model *> models;
    ASSERT_EQ(listen(model.get(), keep, &heard), BIYOS_OK);
    ASSERT_EQ(listen(model.get(), mark, &models), BIYOS_OK);
    const Model other(env);
    std::vector<const BIYOS_model *> stray;
    ASSERT_EQ(listen(other.get(), mark, &stray), BIYOS_OK);
    const std::string path = written("biyos_warned.mps", "");
    ASSERT_EQ(BIYOS_writemdl(model.get(), path.c_str(), BIYOS_FILEFMT_MPS), BIYOS_OK);
    const std::string left = "free MPS leaves out the model options";
    EXPECT_EQ(heard, std::vector{"io/file '" + path + "': " + left + " file=" + path});
    EXPECT_EQ(models, std::vector<const BIYOS_model *>{model.get()});
    EXPECT_TRUE(stray.empty());
}

TEST(CApiTest, ReadWarned)
{
    const Env env;
    const Model model(env);
    std::vector<const BIYOS_model *> own;
    std::vector<const BIYOS_model *> shared;
    ASSERT_EQ(listen(model.get(), mark, &own), BIYOS_OK);
    BIYOS_index id = -1;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), mark, &shared, BIYOS_EVTKND_MSG, BIYOS_EVTLVL_WARN, &id),
              BIYOS_OK);
    const std::string path =
        written("biyos_warned_read.mps",
                "NAME t\nROWS\n N f\nCOLUMNS\n x f 1\nRANGES\n r f 2\nENDATA\n");
    const Read made = read(env, path.c_str(), BIYOS_FILEFMT_MPS);
    ASSERT_EQ(made.status, BIYOS_OK);
    EXPECT_FALSE(shared.empty());
    EXPECT_EQ(shared, std::vector<const BIYOS_model *>(shared.size(), nullptr));
    EXPECT_TRUE(own.empty());
    EXPECT_EQ(BIYOS_freemdl(made.model), BIYOS_OK);
}
