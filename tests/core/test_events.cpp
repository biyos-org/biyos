#include "core/error.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <string>

namespace
{

using biyos::EventKind;
using biyos::Level;

std::string failure(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const biyos::Error &error)
    {
        return error.what();
    }
    return "no error";
}

}

TEST(EventTest, KindIds)
{
    EXPECT_EQ(biyos::find_kind(BIYOS_EVENT_MESSAGE), EventKind::Message);
    EXPECT_EQ(failure([] { biyos::find_kind(0); }), "invalid argument: event kind 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_kind(2); }), "invalid argument: event kind 2: unknown");
}

TEST(EventTest, LevelIds)
{
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_DEBUG), Level::Debug);
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_INFO), Level::Info);
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_WARNING), Level::Warning);
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_ERROR), Level::Error);
    EXPECT_EQ(failure([] { biyos::find_level(0); }), "invalid argument: level 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_level(5); }), "invalid argument: level 5: unknown");
}
