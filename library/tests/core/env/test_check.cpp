#include "core/env/check.hpp"
#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"
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

using biyos::Checker;
using biyos::Checking;
using biyos::Env;
using biyos::Error;
using biyos::ErrorKind;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Field;
using biyos::Lookup;
using biyos::Option;
using biyos::Settings;
using Texts = std::vector<std::string>;

static_assert(!std::is_constructible_v<Checker, Env &, Lookup, std::string>);

Error caught(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const Error &error)
    {
        return error;
    }
    ADD_FAILURE() << "no error thrown";
    return {ErrorKind::Internal, "", "no error"};
}

Texts pairs(const Event &event)
{
    Texts pairs;
    for (const Field &field : event.fields)
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

void finite(const Checker &checker)
{
    checker.require(Checking::Cheap, ErrorKind::Internal, "finite", "x", broken, nothing);
}

class CheckTest : public ::testing::Test
{
  protected:
    Checker at(Checking checking)
    {
        env.options().set_int(Option::Checking, static_cast<int>(checking));
        return Checker(env.stream(), Lookup(env.options()), "model");
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

    void check(const Checker &checker,
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
        const auto store = [this](const Event &event) { kept.push_back(event); };
        env.events().add_hook(kind, EventLevel::Debug, store);
    }

    void reenter(const Checker &checker)
    {
        ++received;
        if (received == 1)
        {
            nested = caught([&] { check(checker, Checking::Cheap, false); }).what();
        }
    }

    Env env;
    int evaluated = 0;
    int described = 0;
    std::vector<Event> kept;
    int received = 0;
    std::string nested;
};

}

TEST_F(CheckTest, DefaultLevel)
{
    const Checker checker(env.stream(), Lookup(env.options()), "model");
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
    const Checker checker = at(Checking::None);
    EXPECT_NO_THROW(check(checker, Checking::Cheap, false));
    EXPECT_NO_THROW(check(checker, Checking::Full, false));
    EXPECT_NO_THROW(check(at(Checking::Full), Checking::None, false));
    EXPECT_EQ(evaluated, 0);
}

TEST_F(CheckTest, ReadOnce)
{
    const Checker checker = at(Checking::None);
    env.options().set_int(Option::Checking, BIYOS_CHK_FULL);
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
    const Error error = caught([this] { check(at(Checking::Cheap), Checking::Cheap, false); });
    EXPECT_EQ(error.kind(), ErrorKind::InvalidArgument);
    EXPECT_STREQ(error.what(), "invalid argument: row 3: check 'sorted' failed: 3 > 2");
    EXPECT_EQ(described, 1);
    ASSERT_EQ(kept.size(), 1U);
    const Event &event = kept.front();
    EXPECT_EQ(event.kind, EventKind::Invariant);
    EXPECT_EQ(event.level, EventLevel::Error);
    EXPECT_EQ(event.module, "model");
    EXPECT_EQ(event.text, error.what());
    EXPECT_EQ(pairs(event), (Texts{"invariant=sorted", "object=row 3", "values=3 > 2"}));
}

TEST_F(CheckTest, LastError)
{
    std::string seen;
    const auto read = [this, &seen](const Event &) { seen = env.last_error().text; };
    env.events().add_hook(EventKind::Invariant, EventLevel::Error, read);
    env.stream().record(ErrorKind::InvalidArgument, "earlier");
    const Error error = caught([this] { check(at(Checking::Cheap), Checking::Cheap, false); });
    EXPECT_EQ(seen, error.what());
    EXPECT_STREQ(env.last_error().text, error.what());
}

TEST_F(CheckTest, CallerKind)
{
    const Checker checker = at(Checking::Cheap);
    for (const ErrorKind kind : {ErrorKind::NoMemory, ErrorKind::Internal})
    {
        EXPECT_EQ(caught([&] { check(checker, Checking::Cheap, false, kind); }).kind(), kind);
    }
}

TEST_F(CheckTest, EmptyValues)
{
    const Checker checker = at(Checking::Cheap);
    const Error error = caught([&checker] { finite(checker); });
    EXPECT_STREQ(error.what(), "internal error: x: check 'finite' failed");
}

TEST_F(CheckTest, Delivering)
{
    const Checker checker = at(Checking::Cheap);
    const auto inner = [this, &checker](const Event &) { reenter(checker); };
    env.events().add_hook(EventKind::Invariant, EventLevel::Debug, inner);
    EXPECT_EQ(caught([&] { check(checker, Checking::Cheap, false); }).kind(),
              ErrorKind::InvalidArgument);
    EXPECT_EQ(received, 1);
    EXPECT_EQ(nested, "invalid argument: row 3: check 'sorted' failed: 3 > 2");
}

TEST_F(CheckTest, Filling)
{
    const Checker checker = at(Checking::Cheap);
    const auto fill = [&](Event &) { check(checker, Checking::Cheap, false); };
    keep(EventKind::Invariant);
    keep(EventKind::Message);
    env.stream().record(ErrorKind::InvalidArgument, "earlier");
    env.stream().emit(EventKind::Message, EventLevel::Info, fill);
    EXPECT_EQ(evaluated, 1);
    EXPECT_TRUE(kept.empty());
    EXPECT_STREQ(env.last_error().text, "earlier");
}

TEST_F(CheckTest, Models)
{
    Settings first;
    const Settings second;
    first.set_int(Option::Checking, BIYOS_CHK_FULL, env.stream());
    const Checker own(env.stream(), Lookup(first, env.options()), "model");
    const Checker other(env.stream(), Lookup(second, env.options()), "model");
    const Checker shared(env.stream(), Lookup(env.options()), "model");
    EXPECT_TRUE(own.active(Checking::Full));
    EXPECT_FALSE(other.active(Checking::Full));
    EXPECT_FALSE(shared.active(Checking::Full));
    EXPECT_THROW(check(own, Checking::Full, false), Error);
    EXPECT_NO_THROW(check(other, Checking::Full, false));
    EXPECT_NO_THROW(check(shared, Checking::Full, false));
    EXPECT_TRUE(other.active(Checking::Cheap));
}
