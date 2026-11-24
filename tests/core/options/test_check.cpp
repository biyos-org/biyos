#include "core/check.hpp"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/events.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{

using biyos::Checking;
using biyos::ErrorKind;
using biyos::EventKind;
using biyos::EventLevel;
using Texts = std::vector<std::string>;

static_assert(!std::is_constructible_v<biyos::Checker, biyos::Env &, biyos::Lookup, std::string>);

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
    checker.require(Checking::Cheap, ErrorKind::Internal, "finite", "x", broken, nothing);
}

class CheckTest : public ::testing::Test
{
  protected:
    biyos::Checker at(Checking checking)
    {
        env.options().set_int(biyos::Option::Checking, static_cast<int>(checking));
        return biyos::Checker(env, biyos::Lookup(env.options()), "model");
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
               Checking cost,
               bool result,
               ErrorKind kind = ErrorKind::InvalidArgument)
    {
        const auto holds = [this, result] { return evaluate(result); };
        const auto values = [this] { return describe(); };
        checker.require(cost, kind, "sorted", "row 3", holds, values);
    }

    void keep(EventKind kind)
    {
        const auto store = [this](const biyos::Event &event) { kept.push_back(event); };
        env.events().add_hook(kind, EventLevel::Debug, store);
    }

    void reenter(const biyos::Checker &checker)
    {
        ++received;
        if (received == 1)
        {
            nested = caught([&] { check(checker, Checking::Cheap, false); }).what();
        }
    }

    biyos::Env env;
    int evaluated = 0;
    int described = 0;
    std::vector<biyos::Event> kept;
    int received = 0;
    std::string nested;
};

}

TEST_F(CheckTest, DefaultLevel)
{
    const biyos::Checker checker(env, biyos::Lookup(env.options()), "model");
    EXPECT_TRUE(checker.active(Checking::Cheap));
    EXPECT_FALSE(checker.active(Checking::Full));
}

TEST_F(CheckTest, BelowLevel)
{
    EXPECT_NO_THROW(check(at(Checking::Cheap), Checking::Full, false));
    EXPECT_EQ(evaluated, 0);
    check(at(Checking::Cheap), Checking::Cheap, true);
    EXPECT_EQ(evaluated, 1);
    check(at(Checking::Full), Checking::Cheap, true);
    check(at(Checking::Full), Checking::Full, true);
    EXPECT_EQ(evaluated, 3);
}

TEST_F(CheckTest, NoneLevel)
{
    const biyos::Checker checker = at(Checking::None);
    EXPECT_NO_THROW(check(checker, Checking::Cheap, false));
    EXPECT_NO_THROW(check(checker, Checking::Full, false));
    EXPECT_NO_THROW(check(at(Checking::Full), Checking::None, false));
    EXPECT_EQ(evaluated, 0);
}

TEST_F(CheckTest, ReadOnce)
{
    const biyos::Checker checker = at(Checking::None);
    env.options().set_int(biyos::Option::Checking, BIYOS_CHK_FULL);
    EXPECT_NO_THROW(check(checker, Checking::Cheap, false));
    EXPECT_EQ(evaluated, 0);
}

TEST_F(CheckTest, LazyValues)
{
    check(at(Checking::Full), Checking::Cheap, true);
    check(at(Checking::Full), Checking::Full, true);
    EXPECT_EQ(evaluated, 2);
    EXPECT_EQ(described, 0);
}

TEST_F(CheckTest, OneEvent)
{
    keep(EventKind::Invariant);
    keep(EventKind::Message);
    const biyos::Error error =
        caught([this] { check(at(Checking::Cheap), Checking::Cheap, false); });
    EXPECT_EQ(error.kind(), ErrorKind::InvalidArgument);
    EXPECT_STREQ(error.what(), "invalid argument: row 3: check 'sorted' failed: 3 > 2");
    EXPECT_EQ(described, 1);
    ASSERT_EQ(kept.size(), 1U);
    const biyos::Event &event = kept.front();
    EXPECT_EQ(event.kind, EventKind::Invariant);
    EXPECT_EQ(event.level, EventLevel::Error);
    EXPECT_EQ(event.module, "model");
    EXPECT_EQ(event.text, error.what());
    EXPECT_EQ(pairs(event), (Texts{"invariant=sorted", "object=row 3", "values=3 > 2"}));
}

TEST_F(CheckTest, LastError)
{
    std::string seen;
    const auto read = [this, &seen](const biyos::Event &) { seen = env.last_error().text; };
    env.events().add_hook(EventKind::Invariant, EventLevel::Error, read);
    env.record_error(biyos::ErrorKind::InvalidArgument, "earlier");
    const biyos::Error error =
        caught([this] { check(at(Checking::Cheap), Checking::Cheap, false); });
    EXPECT_EQ(seen, error.what());
    EXPECT_STREQ(env.last_error().text, error.what());
}

TEST_F(CheckTest, CallerKind)
{
    const biyos::Checker checker = at(Checking::Cheap);
    for (const ErrorKind kind : {ErrorKind::NoMemory, ErrorKind::Internal})
    {
        EXPECT_EQ(caught([&] { check(checker, Checking::Cheap, false, kind); }).kind(), kind);
    }
}

TEST_F(CheckTest, EmptyValues)
{
    const biyos::Checker checker = at(Checking::Cheap);
    const biyos::Error error = caught([&checker] { finite(checker); });
    EXPECT_STREQ(error.what(), "internal error: x: check 'finite' failed");
}

TEST_F(CheckTest, Delivering)
{
    const biyos::Checker checker = at(Checking::Cheap);
    const auto inner = [this, &checker](const biyos::Event &) { reenter(checker); };
    env.events().add_hook(EventKind::Invariant, EventLevel::Debug, inner);
    EXPECT_EQ(caught([&] { check(checker, Checking::Cheap, false); }).kind(),
              ErrorKind::InvalidArgument);
    EXPECT_EQ(received, 1);
    EXPECT_EQ(nested, "invalid argument: row 3: check 'sorted' failed: 3 > 2");
}

TEST_F(CheckTest, Filling)
{
    const biyos::Checker checker = at(Checking::Cheap);
    const auto fill = [&](biyos::Event &) { check(checker, Checking::Cheap, false); };
    keep(EventKind::Invariant);
    keep(EventKind::Message);
    env.record_error(biyos::ErrorKind::InvalidArgument, "earlier");
    env.events().emit(EventKind::Message, EventLevel::Info, fill);
    EXPECT_EQ(evaluated, 1);
    EXPECT_TRUE(kept.empty());
    EXPECT_STREQ(env.last_error().text, "earlier");
}

TEST_F(CheckTest, Models)
{
    biyos::Settings first;
    const biyos::Settings second;
    first.set_int(biyos::Option::Checking, BIYOS_CHK_FULL, env.events());
    const biyos::Checker own(env, biyos::Lookup(first, env.options()), "model");
    const biyos::Checker other(env, biyos::Lookup(second, env.options()), "model");
    const biyos::Checker shared(env, biyos::Lookup(env.options()), "model");
    EXPECT_TRUE(own.active(Checking::Full));
    EXPECT_FALSE(other.active(Checking::Full));
    EXPECT_FALSE(shared.active(Checking::Full));
    EXPECT_THROW(check(own, Checking::Full, false), biyos::Error);
    EXPECT_NO_THROW(check(other, Checking::Full, false));
    EXPECT_NO_THROW(check(shared, Checking::Full, false));
    EXPECT_TRUE(other.active(Checking::Cheap));
}
