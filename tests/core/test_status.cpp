#include "biyos/status.h"
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

std::string_view termination_name(int code)
{
    return biyos::name_of(static_cast<TerminationStatus>(code));
}

std::string_view result_name(int code)
{
    return biyos::name_of(static_cast<ResultStatus>(code));
}

std::string_view claim_name(int code)
{
    return biyos::name_of(static_cast<Claim>(code));
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

TEST(StatusTest, TerminationNames)
{
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_NOT_SOLVED), "not solved");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_OPTIMAL), "optimal");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_LOCALLY_SOLVED), "locally solved");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_ALMOST_LOCALLY_SOLVED), "almost locally solved");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_ITERATION_LIMIT), "iteration limit");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_TIME_LIMIT), "time limit");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_CPU_TIME_LIMIT), "CPU time limit");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_EVALUATION_LIMIT), "evaluation limit");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_USER_STOP), "user stop");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_INFEASIBLE), "infeasible");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_LOCALLY_INFEASIBLE), "locally infeasible");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_UNBOUNDED), "unbounded");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_SLOW_PROGRESS), "slow progress");
    EXPECT_EQ(termination_name(BIYOS_TERMINATION_NUMERICAL_TROUBLE), "numerical trouble");
}

TEST(StatusTest, ResultNames)
{
    EXPECT_EQ(result_name(BIYOS_RESULT_NONE), "none");
    EXPECT_EQ(result_name(BIYOS_RESULT_FEASIBLE), "feasible");
    EXPECT_EQ(result_name(BIYOS_RESULT_NEARLY_FEASIBLE), "nearly feasible");
    EXPECT_EQ(result_name(BIYOS_RESULT_INFEASIBLE), "infeasible");
    EXPECT_EQ(result_name(BIYOS_RESULT_CERTIFICATE), "certificate");
    EXPECT_EQ(result_name(BIYOS_RESULT_NEARLY_CERTIFICATE), "nearly certificate");
    EXPECT_EQ(result_name(BIYOS_RESULT_UNKNOWN), "unknown");
}

TEST(StatusTest, ClaimNames)
{
    EXPECT_EQ(claim_name(BIYOS_CLAIM_NONE), "none");
    EXPECT_EQ(claim_name(BIYOS_CLAIM_LOCAL), "local");
    EXPECT_EQ(claim_name(BIYOS_CLAIM_GLOBAL), "global");
}
