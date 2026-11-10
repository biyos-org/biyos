#include "core/status.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <set>
#include <string_view>

namespace
{

using biyos::Claim;
using biyos::ResultStatus;
using biyos::TerminationStatus;

constexpr int kLargest = 999;

constexpr std::array kTerminations{
    TerminationStatus::NotSolved,         TerminationStatus::Optimal,
    TerminationStatus::LocallySolved,     TerminationStatus::AlmostLocallySolved,
    TerminationStatus::IterationLimit,    TerminationStatus::TimeLimit,
    TerminationStatus::CpuTimeLimit,      TerminationStatus::EvaluationLimit,
    TerminationStatus::UserStop,          TerminationStatus::Infeasible,
    TerminationStatus::LocallyInfeasible, TerminationStatus::Unbounded,
    TerminationStatus::SlowProgress,      TerminationStatus::NumericalTrouble,
};

constexpr std::array kResults{
    ResultStatus::None,       ResultStatus::Feasible,    ResultStatus::NearlyFeasible,
    ResultStatus::Infeasible, ResultStatus::Certificate, ResultStatus::NearlyCertificate,
    ResultStatus::Unknown,
};

constexpr std::array kClaims{
    Claim::None,
    Claim::Local,
    Claim::Global,
};

enum class Group
{
    None = 0,
    Solved = 1,
    Stopped = 2,
    Detected = 3,
    Stalled = 4,
};

Group group_of(TerminationStatus status)
{
    switch (status)
    {
    case TerminationStatus::NotSolved:
        return Group::None;
    case TerminationStatus::Optimal:
    case TerminationStatus::LocallySolved:
    case TerminationStatus::AlmostLocallySolved:
        return Group::Solved;
    case TerminationStatus::IterationLimit:
    case TerminationStatus::TimeLimit:
    case TerminationStatus::CpuTimeLimit:
    case TerminationStatus::EvaluationLimit:
    case TerminationStatus::UserStop:
        return Group::Stopped;
    case TerminationStatus::Infeasible:
    case TerminationStatus::LocallyInfeasible:
    case TerminationStatus::Unbounded:
        return Group::Detected;
    case TerminationStatus::SlowProgress:
    case TerminationStatus::NumericalTrouble:
        return Group::Stalled;
    }
    return Group::None;
}

template <typename Status, std::size_t N> void expect_distinct(const std::array<Status, N> &all)
{
    std::set<int> codes;
    std::set<std::string_view> names;
    for (const Status status : all)
    {
        codes.insert(static_cast<int>(status));
        names.insert(biyos::name_of(status));
    }
    EXPECT_EQ(codes.size(), N);
    EXPECT_EQ(names.size(), N);
    EXPECT_FALSE(names.contains(""));
}

template <typename Status, std::size_t N> void expect_listed(const std::array<Status, N> &all)
{
    for (int code = -kLargest; code <= kLargest; ++code)
    {
        const Status status = static_cast<Status>(code);
        const bool named = !biyos::name_of(status).empty();
        const bool listed = std::ranges::find(all, status) != all.end();
        EXPECT_EQ(named, listed) << "code " << code;
    }
}

}

TEST(StatusTest, Termination)
{
    expect_distinct(kTerminations);
    expect_listed(kTerminations);
}

TEST(StatusTest, Result)
{
    expect_distinct(kResults);
    expect_listed(kResults);
}

TEST(StatusTest, Claim)
{
    expect_distinct(kClaims);
    expect_listed(kClaims);
}

TEST(StatusTest, Groups)
{
    for (const TerminationStatus status : kTerminations)
    {
        const int group = static_cast<int>(group_of(status));
        EXPECT_EQ(static_cast<int>(status) / 100, group) << biyos::name_of(status);
    }
}

TEST(StatusTest, Names)
{
    EXPECT_EQ(biyos::name_of(TerminationStatus::AlmostLocallySolved), "almost locally solved");
    EXPECT_EQ(biyos::name_of(TerminationStatus::CpuTimeLimit), "CPU time limit");
    EXPECT_EQ(biyos::name_of(ResultStatus::NearlyFeasible), "nearly feasible");
    EXPECT_EQ(biyos::name_of(Claim::Global), "global");
    EXPECT_EQ(biyos::name_of(static_cast<TerminationStatus>(104)), "");
}
