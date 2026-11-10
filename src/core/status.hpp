#pragma once

#include "biyos/status.h"

#include <string_view>

namespace biyos
{

enum class TerminationStatus
{
    NotSolved = BIYOS_TERMINATION_NOT_SOLVED,
    Optimal = BIYOS_TERMINATION_OPTIMAL,
    LocallySolved = BIYOS_TERMINATION_LOCALLY_SOLVED,
    AlmostLocallySolved = BIYOS_TERMINATION_ALMOST_LOCALLY_SOLVED,
    IterationLimit = BIYOS_TERMINATION_ITERATION_LIMIT,
    TimeLimit = BIYOS_TERMINATION_TIME_LIMIT,
    CpuTimeLimit = BIYOS_TERMINATION_CPU_TIME_LIMIT,
    EvaluationLimit = BIYOS_TERMINATION_EVALUATION_LIMIT,
    UserStop = BIYOS_TERMINATION_USER_STOP,
    Infeasible = BIYOS_TERMINATION_INFEASIBLE,
    LocallyInfeasible = BIYOS_TERMINATION_LOCALLY_INFEASIBLE,
    Unbounded = BIYOS_TERMINATION_UNBOUNDED,
    SlowProgress = BIYOS_TERMINATION_SLOW_PROGRESS,
    NumericalTrouble = BIYOS_TERMINATION_NUMERICAL_TROUBLE,
};

enum class ResultStatus
{
    None = BIYOS_RESULT_NONE,
    Feasible = BIYOS_RESULT_FEASIBLE,
    NearlyFeasible = BIYOS_RESULT_NEARLY_FEASIBLE,
    Infeasible = BIYOS_RESULT_INFEASIBLE,
    Certificate = BIYOS_RESULT_CERTIFICATE,
    NearlyCertificate = BIYOS_RESULT_NEARLY_CERTIFICATE,
    Unknown = BIYOS_RESULT_UNKNOWN,
};

enum class Claim
{
    None = BIYOS_CLAIM_NONE,
    Local = BIYOS_CLAIM_LOCAL,
    Global = BIYOS_CLAIM_GLOBAL,
};

std::string_view name_of(TerminationStatus status) noexcept;
std::string_view name_of(ResultStatus status) noexcept;
std::string_view name_of(Claim claim) noexcept;

}
