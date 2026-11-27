#include "sample.hpp"
#include "texts.hpp"

#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "io/files.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

using biyos::difference;
using biyos::Env;
using biyos::Error;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::FileFormat;
using biyos::Index;
using biyos::Model;
using biyos::Option;
using biyos::read_file;
using biyos::Settings;
using biyos::write_file;
using biyos::test::built;
using biyos::test::sample;

namespace
{

std::string path_of(const std::string &name)
{
    return testing::TempDir() + name;
}

std::string refusal(const Model &model, const std::string &path)
{
    Env env;
    try
    {
        write_file(env.stream(), model, Settings(), path, FileFormat::Bys);
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "";
}

std::vector<std::string> heard(const Model &model, const Settings &settings)
{
    Env env;
    std::vector<std::string> texts;
    const auto record = [&texts](const Event &event) { texts.push_back(event.text); };
    env.events().add_hook(EventKind::Message, EventLevel::Warning, record);
    write_file(env.stream(), model, settings, path_of("biyos_heard.bys"), FileFormat::Bys);
    return texts;
}

}

TEST(NativeTest, File)
{
    const Model model = built(sample());
    const std::string path = path_of("biyos_sample.bys");
    Env env;
    write_file(env.stream(), model, Settings(), path, FileFormat::Bys);
    EXPECT_EQ(difference(read_file(env, path, FileFormat::Bys), model), "");
}

TEST(NativeTest, Callbacks)
{
    Model model = built(sample());
    const std::array functions{Index{1}};
    model.add_callback(functions, nullptr);
    const std::string path = path_of("biyos_callback.bys");
    std::filesystem::remove(path);
    EXPECT_EQ(refusal(model, path),
              "unsupported: callback 0: the native format cannot hold callbacks");
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(NativeTest, Options)
{
    const Model model = built(sample());
    EXPECT_TRUE(heard(model, Settings()).empty());
    Env env;
    Settings settings;
    settings.set_int(Option::Seed, 3, env.stream());
    const std::string file = path_of("biyos_heard.bys");
    EXPECT_EQ(heard(model, settings),
              std::vector<std::string>{"file '" + file +
                                       "': the native format leaves out the model options"});
}
