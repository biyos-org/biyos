#pragma once

#include "biyos/biyos.h"

#include <string_view>

namespace biyos
{

enum class TerminationStatus
{
    NotSolved = BIYOS_TRM_NONE,
    Optimal = BIYOS_TRM_OPTIMAL,
    LocallySolved = BIYOS_TRM_LOCAL,
    AlmostLocallySolved = BIYOS_TRM_NEARLOCAL,
    IterationLimit = BIYOS_TRM_ITERLIM,
    TimeLimit = BIYOS_TRM_TIMELIM,
    CpuTimeLimit = BIYOS_TRM_CPULIM,
    EvaluationLimit = BIYOS_TRM_EVALLIM,
    UserStop = BIYOS_TRM_STOP,
    Infeasible = BIYOS_TRM_INFEAS,
    LocallyInfeasible = BIYOS_TRM_LOCINFEAS,
    Unbounded = BIYOS_TRM_UNBND,
    SlowProgress = BIYOS_TRM_SLOW,
    NumericalTrouble = BIYOS_TRM_NUMERIC,
};

enum class ResultStatus
{
    None = BIYOS_RES_NONE,
    Feasible = BIYOS_RES_FEAS,
    NearlyFeasible = BIYOS_RES_NEARFEAS,
    Infeasible = BIYOS_RES_INFEAS,
    Certificate = BIYOS_RES_CER,
    NearlyCertificate = BIYOS_RES_NEARCER,
    Unknown = BIYOS_RES_UNKNOWN,
};

enum class Claim
{
    None = BIYOS_CLM_NONE,
    Local = BIYOS_CLM_LOCAL,
    Global = BIYOS_CLM_GLOBAL,
};

std::string_view name_of(TerminationStatus status) noexcept;
std::string_view name_of(ResultStatus status) noexcept;
std::string_view name_of(Claim claim) noexcept;

}
