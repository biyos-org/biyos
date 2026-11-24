#pragma once

#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::test
{

struct ToleranceCalls
{
    std::string_view noun;
    decltype(&BIYOS_getnvar) count;
    decltype(&BIYOS_setvartolsp) set_listed;
    decltype(&BIYOS_setvartolrg) set_ranged;
    decltype(&BIYOS_getvartolsp) get_listed;
    decltype(&BIYOS_getvartolrg) get_ranged;
    decltype(&BIYOS_delvartolsp) delete_listed;
    decltype(&BIYOS_delvartolrg) delete_ranged;
};

inline const ToleranceCalls kVariableTolerances{
    .noun = "variable",
    .count = BIYOS_getnvar,
    .set_listed = BIYOS_setvartolsp,
    .set_ranged = BIYOS_setvartolrg,
    .get_listed = BIYOS_getvartolsp,
    .get_ranged = BIYOS_getvartolrg,
    .delete_listed = BIYOS_delvartolsp,
    .delete_ranged = BIYOS_delvartolrg,
};

inline const ToleranceCalls kConstraintTolerances{
    .noun = "constraint",
    .count = BIYOS_getncon,
    .set_listed = BIYOS_setcontolsp,
    .set_ranged = BIYOS_setcontolrg,
    .get_listed = BIYOS_getcontolsp,
    .get_ranged = BIYOS_getcontolrg,
    .delete_listed = BIYOS_delcontolsp,
    .delete_ranged = BIYOS_delcontolrg,
};

inline const ToleranceCalls kConeTolerances{
    .noun = "cone",
    .count = BIYOS_getncone,
    .set_listed = BIYOS_setconetolsp,
    .set_ranged = BIYOS_setconetolrg,
    .get_listed = BIYOS_getconetolsp,
    .get_ranged = BIYOS_getconetolrg,
    .delete_listed = BIYOS_delconetolsp,
    .delete_ranged = BIYOS_delconetolrg,
};

using Fill = void (*)(const Model &);

struct Tolerances
{
    std::vector<double> absolute;
    std::vector<double> relative;
};

inline constexpr double kUnknown = std::numeric_limits<double>::quiet_NaN();

inline const Tolerances kUnset{
    .absolute = {kUnknown, kUnknown, kUnknown},
    .relative = {kUnknown, kUnknown, kUnknown},
};

inline Tolerances read_tolerances(const Model &model, const ToleranceCalls &calls)
{
    BIYOS_index n = -1;
    EXPECT_EQ(calls.count(model.get(), &n), BIYOS_OK);
    const auto size = static_cast<std::size_t>(n);
    Tolerances read{
        .absolute = std::vector<double>(size, 9.0),
        .relative = std::vector<double>(size, 9.0),
    };
    const BIYOS_status status =
        calls.get_ranged(model.get(), 0, n, read.absolute.data(), read.relative.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

inline std::vector<std::uint64_t> bit_patterns(const std::vector<double> &values)
{
    std::vector<std::uint64_t> bits;
    bits.reserve(values.size());
    for (const double value : values)
    {
        bits.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return bits;
}

inline void expect_tolerances(const Tolerances &found, const Tolerances &wanted)
{
    EXPECT_EQ(bit_patterns(found.absolute), bit_patterns(wanted.absolute));
    EXPECT_EQ(bit_patterns(found.relative), bit_patterns(wanted.relative));
}

inline std::string refusal_of(const ToleranceCalls &calls, int index, std::string_view reason)
{
    return "invalid argument: " + std::string(calls.noun) + " " + std::to_string(index) + ": " +
           std::string(reason);
}

inline void expect_own(const ToleranceCalls &calls, Fill fill)
{
    const Env env;
    const Model model(env);
    fill(model);
    expect_tolerances(read_tolerances(model, calls), kUnset);
    BIYOS_model *held = model.get();
    const BIYOS_index indices[] = {2, 0};
    const double absolute[] = {0.5, 0.0};
    const double relative[] = {0.25, 0.125};
    ASSERT_EQ(calls.set_listed(held, 2, indices, absolute, nullptr), BIYOS_OK);
    ASSERT_EQ(calls.set_ranged(held, 1, 2, nullptr, relative), BIYOS_OK);
    ASSERT_EQ(calls.set_ranged(held, 0, 3, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(calls.set_listed(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    const Tolerances wanted{
        .absolute = {0.0, kUnknown, 0.5},
        .relative = {kUnknown, 0.25, 0.125},
    };
    expect_tolerances(read_tolerances(model, calls), wanted);
    const BIYOS_index repeated[] = {1, 2, 1};
    std::vector<double> listed(3, 9.0);
    ASSERT_EQ(calls.get_listed(held, 3, repeated, nullptr, listed.data()), BIYOS_OK);
    EXPECT_EQ(bit_patterns(listed), bit_patterns({0.25, 0.125, 0.25}));
    EXPECT_EQ(calls.get_listed(held, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
}

inline void expect_rejected(const ToleranceCalls &calls, Fill fill)
{
    const Env env;
    const Model model(env);
    fill(model);
    BIYOS_model *held = model.get();
    const BIYOS_index index = 1;
    const double nan = kUnknown;
    const double infinite = BIYOS_INF;
    const double negative = -0.5;
    const double small = 0.5;
    EXPECT_EQ(calls.set_listed(held, 1, &index, &nan, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 1, "absolute tolerance is NaN"));
    EXPECT_EQ(calls.set_listed(held, 1, &index, nullptr, &infinite), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 1, "relative tolerance is infinity"));
    EXPECT_EQ(calls.set_ranged(held, 1, 1, &negative, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 1, "absolute tolerance -0.5 is negative"));
    const BIYOS_index unknown = 3;
    EXPECT_EQ(calls.set_listed(held, 1, &unknown, &small, &small), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 3, "unknown"));
    EXPECT_EQ(calls.set_listed(held, 1, nullptr, &small, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    const BIYOS_index indices[] = {0, 2};
    const double absolute[] = {0.5, kUnknown};
    EXPECT_EQ(calls.set_listed(held, 2, indices, absolute, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 2, "absolute tolerance is NaN"));
    std::vector<double> kept(2, 9.0);
    EXPECT_EQ(calls.get_ranged(held, 2, 2, kept.data(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 3, "unknown"));
    EXPECT_EQ(bit_patterns(kept), bit_patterns({9.0, 9.0}));
    expect_tolerances(read_tolerances(model, calls), kUnset);
}

inline void expect_deleted(const ToleranceCalls &calls, Fill fill)
{
    const Env env;
    const Model model(env);
    fill(model);
    BIYOS_model *held = model.get();
    const double absolute[] = {0.5, 0.25, 0.125};
    const double relative[] = {0.75, 0.375, 0.0};
    ASSERT_EQ(calls.set_ranged(held, 0, 3, absolute, relative), BIYOS_OK);
    const BIYOS_index indices[] = {2, 2};
    ASSERT_EQ(calls.delete_listed(held, 2, indices), BIYOS_OK);
    ASSERT_EQ(calls.delete_listed(held, 0, nullptr), BIYOS_OK);
    const Tolerances wanted{
        .absolute = {0.5, 0.25, kUnknown},
        .relative = {0.75, 0.375, kUnknown},
    };
    expect_tolerances(read_tolerances(model, calls), wanted);
    ASSERT_EQ(calls.delete_ranged(held, 0, 2), BIYOS_OK);
    ASSERT_EQ(calls.delete_ranged(held, 1, 2), BIYOS_OK);
    expect_tolerances(read_tolerances(model, calls), kUnset);
}

inline void expect_undeleted(const ToleranceCalls &calls, Fill fill)
{
    const Env env;
    const Model model(env);
    fill(model);
    BIYOS_model *held = model.get();
    const double absolute[] = {0.5, 0.25, 0.125};
    ASSERT_EQ(calls.set_ranged(held, 0, 3, absolute, nullptr), BIYOS_OK);
    const Tolerances before = read_tolerances(model, calls);
    const BIYOS_index indices[] = {0, 3};
    EXPECT_EQ(calls.delete_listed(held, 2, indices), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 3, "unknown"));
    EXPECT_EQ(calls.delete_listed(held, 1, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(calls.delete_ranged(held, 2, 2), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), refusal_of(calls, 3, "unknown"));
    expect_tolerances(read_tolerances(model, calls), before);
}

inline void expect_copied(const ToleranceCalls &calls, Fill fill)
{
    const Env env;
    const Model model(env);
    fill(model);
    const double absolute[] = {0.5, 0.25};
    const double relative[] = {0.0, 0.125};
    ASSERT_EQ(calls.set_ranged(model.get(), 1, 2, absolute, relative), BIYOS_OK);
    const Tolerances wanted = read_tolerances(model, calls);
    const Env other;
    const Model copy(other, model);
    expect_tolerances(read_tolerances(copy, calls), wanted);
    ASSERT_EQ(calls.delete_ranged(copy.get(), 0, 3), BIYOS_OK);
    expect_tolerances(read_tolerances(copy, calls), kUnset);
    expect_tolerances(read_tolerances(model, calls), wanted);
}

struct Locked
{
    const ToleranceCalls *calls = nullptr;
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
};

inline void keep_status(Locked &locked, BIYOS_status status)
{
    locked.statuses.push_back(status);
    locked.messages.emplace_back(last_message(locked.env));
}

inline void try_tolerances(const BIYOS_event *, void *data)
{
    Locked &locked = *static_cast<Locked *>(data);
    const ToleranceCalls &calls = *locked.calls;
    const BIYOS_index index = 0;
    const double value = 0.5;
    keep_status(locked, calls.set_listed(locked.model, 1, &index, &value, &value));
    keep_status(locked, calls.get_listed(locked.model, 1, &index, nullptr, nullptr));
    keep_status(locked, calls.delete_listed(locked.model, 1, &index));
}

inline void expect_locked(const ToleranceCalls &calls, Fill fill)
{
    const Env env;
    const Model model(env);
    fill(model);
    Locked locked{
        .calls = &calls,
        .env = env.get(),
        .model = model.get(),
        .statuses = {},
        .messages = {},
    };
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), try_tolerances, &locked, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG, BIYOS_OK, BIYOS_ERR_BADARG};
    const std::string read_only = "invalid argument: model: is read-only while delivering an event";
    EXPECT_EQ(locked.statuses, statuses);
    EXPECT_EQ(locked.messages, std::vector<std::string>(3, read_only));
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    expect_tolerances(read_tolerances(model, calls), kUnset);
}

inline void check_tolerances(const ToleranceCalls &calls, Fill fill)
{
    expect_own(calls, fill);
    expect_rejected(calls, fill);
    expect_deleted(calls, fill);
    expect_undeleted(calls, fill);
    expect_copied(calls, fill);
    expect_locked(calls, fill);
}

}
