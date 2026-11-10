#include "core/environment.hpp"

#include <gtest/gtest.h>

TEST(EnvironmentTest, NoError)
{
    const biyos::Environment env;
    EXPECT_EQ(env.last_error(), "");
}

TEST(EnvironmentTest, LastError)
{
    biyos::Environment env;
    env.record_error("first");
    env.record_error("second");
    EXPECT_EQ(env.last_error(), "second");
}

TEST(EnvironmentTest, OwnsOptions)
{
    biyos::Environment env;
    env.options().set_int(biyos::Option::Seed, 9);
    const biyos::Environment &view = env;
    EXPECT_EQ(view.options().get_int(biyos::Option::Seed), 9);
}

TEST(EnvironmentTest, OwnsEvents)
{
    biyos::Environment env;
    env.events().subscribe(biyos::EventKind::Message, biyos::Level::Info,
                           [](const biyos::Event &) {});
    const biyos::Environment &view = env;
    EXPECT_TRUE(view.events().wanted(biyos::EventKind::Message, biyos::Level::Info));
}
