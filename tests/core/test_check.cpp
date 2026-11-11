#include "core/check.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "core/options.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using biyos::CheckLevel;
using biyos::ErrorKind;
using biyos::EventKind;
using biyos::Level;
using Texts = std::vector<std::string>;

biyos::Error caught(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const biyos::Error &error)
    {
        return error;
    }
    ADD_FAILURE() << "no error thrown";
    return {ErrorKind::Internal, "", "no error"};
}

Texts pairs(const biyos::Event &event)
{
    Texts pairs;
    for (const biyos::Field &field : event.fields)
    {
        pairs.push_back(field.name + "=" + field.value);
    }
    return pairs;
}

bool broken()
{
    return false;
}

std::string_view nothing()
{
    return {};
}

void finite(const biyos::Checker &checker)
{
    checker.require(CheckLevel::Cheap, ErrorKind::Internal, "finite", "x", broken, nothing);
}

class CheckTest : public ::testing::Test
{
  protected:
    biyos::Checker at(const std::string &level)
    {
        env.options().set_string(biyos::Option::CheckLevel, level);
        return biyos::Checker(env, "model");
    }

    bool evaluate(bool result)
    {
        ++evaluated;
        return result;
    }

    std::string describe()
    {
        ++described;
        return "3 > 2";
    }

    void check(const biyos::Checker &checker,
               CheckLevel level,
               bool result,
               ErrorKind kind = ErrorKind::InvalidArgument)
    {
        const auto holds = [this, result] { return evaluate(result); };
        const auto values = [this] { return describe(); };
        checker.require(level, kind, "sorted", "row 3", holds, values);
    }

    void keep(EventKind kind)
    {
        const auto store = [this](const biyos::Event &event) { kept.push_back(event); };
        env.events().subscribe(kind, Level::Debug, store);
    }

    void reenter(const biyos::Checker &checker)
    {
        ++received;
        if (received == 1)
        {
            nested = caught([&] { check(checker, CheckLevel::Cheap, false); }).what();
        }
    }

    biyos::Environment env;
    int evaluated = 0;
    int described = 0;
    std::vector<biyos::Event> kept;
    int received = 0;
    std::string nested;
};

}

TEST_F(CheckTest, DefaultLevel)
{
    const biyos::Checker checker(env, "model");
    EXPECT_TRUE(checker.active(CheckLevel::Cheap));
    EXPECT_FALSE(checker.active(CheckLevel::Full));
}

TEST_F(CheckTest, BelowLevel)
{
    EXPECT_NO_THROW(check(at("cheap"), CheckLevel::Full, false));
    EXPECT_EQ(evaluated, 0);
    check(at("cheap"), CheckLevel::Cheap, true);
    EXPECT_EQ(evaluated, 1);
    check(at("full"), CheckLevel::Cheap, true);
    check(at("full"), CheckLevel::Full, true);
    EXPECT_EQ(evaluated, 3);
}

TEST_F(CheckTest, NoneLevel)
{
    const biyos::Checker checker = at("none");
    EXPECT_NO_THROW(check(checker, CheckLevel::Cheap, false));
    EXPECT_NO_THROW(check(checker, CheckLevel::Full, false));
    EXPECT_NO_THROW(check(at("full"), CheckLevel::None, false));
    EXPECT_EQ(evaluated, 0);
}

TEST_F(CheckTest, ReadOnce)
{
    const biyos::Checker checker = at("none");
    env.options().set_string(biyos::Option::CheckLevel, "full");
    EXPECT_NO_THROW(check(checker, CheckLevel::Cheap, false));
    EXPECT_EQ(evaluated, 0);
}

TEST_F(CheckTest, LazyValues)
{
    check(at("full"), CheckLevel::Cheap, true);
    check(at("full"), CheckLevel::Full, true);
    EXPECT_EQ(evaluated, 2);
    EXPECT_EQ(described, 0);
}

TEST_F(CheckTest, OneEvent)
{
    keep(EventKind::Check);
    keep(EventKind::Message);
    const biyos::Error error = caught([this] { check(at("cheap"), CheckLevel::Cheap, false); });
    EXPECT_EQ(error.kind(), ErrorKind::InvalidArgument);
    EXPECT_STREQ(error.what(), "invalid argument: row 3: check 'sorted' failed: 3 > 2");
    EXPECT_EQ(described, 1);
    ASSERT_EQ(kept.size(), 1U);
    const biyos::Event &event = kept.front();
    EXPECT_EQ(event.kind, EventKind::Check);
    EXPECT_EQ(event.level, Level::Error);
    EXPECT_EQ(event.module, "model");
    EXPECT_EQ(event.text, error.what());
    EXPECT_EQ(pairs(event), (Texts{"invariant=sorted", "object=row 3", "values=3 > 2"}));
}

TEST_F(CheckTest, LastError)
{
    std::string seen;
    const auto read = [this, &seen](const biyos::Event &) { seen = env.last_error(); };
    env.events().subscribe(EventKind::Check, Level::Error, read);
    env.record_error("earlier");
    const biyos::Error error = caught([this] { check(at("cheap"), CheckLevel::Cheap, false); });
    EXPECT_EQ(seen, error.what());
    EXPECT_EQ(env.last_error(), error.what());
}

TEST_F(CheckTest, CallerKind)
{
    const biyos::Checker checker = at("cheap");
    for (const ErrorKind kind : {ErrorKind::NoMemory, ErrorKind::Internal})
    {
        EXPECT_EQ(caught([&] { check(checker, CheckLevel::Cheap, false, kind); }).kind(), kind);
    }
}

TEST_F(CheckTest, EmptyValues)
{
    const biyos::Checker checker = at("cheap");
    const biyos::Error error = caught([&checker] { finite(checker); });
    EXPECT_STREQ(error.what(), "internal error: x: check 'finite' failed");
}

TEST_F(CheckTest, Delivering)
{
    const biyos::Checker checker = at("cheap");
    const auto inner = [this, &checker](const biyos::Event &) { reenter(checker); };
    env.events().subscribe(EventKind::Check, Level::Debug, inner);
    EXPECT_EQ(caught([&] { check(checker, CheckLevel::Cheap, false); }).kind(),
              ErrorKind::InvalidArgument);
    EXPECT_EQ(received, 1);
    EXPECT_EQ(nested, "invalid argument: row 3: check 'sorted' failed: 3 > 2");
}

TEST_F(CheckTest, Filling)
{
    const biyos::Checker checker = at("cheap");
    const auto fill = [&](biyos::Event &) { check(checker, CheckLevel::Cheap, false); };
    keep(EventKind::Check);
    keep(EventKind::Message);
    env.record_error("earlier");
    env.events().emit(EventKind::Message, Level::Info, fill);
    EXPECT_EQ(evaluated, 1);
    EXPECT_TRUE(kept.empty());
    EXPECT_EQ(env.last_error(), "earlier");
}
