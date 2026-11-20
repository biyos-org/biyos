#pragma once

#include "biyos/status.h"

#include <string_view>

namespace biyos
{

enum class TerminationStatus
{
    NotSolved = BIYOS_TERM_NOTSOLVED,
    Optimal = BIYOS_TERM_OPTIMAL,
    LocallySolved = BIYOS_TERM_LOCALLYSOLVED,
    AlmostLocallySolved = BIYOS_TERM_ALMOSTLOCALLYSOLVED,
    IterationLimit = BIYOS_TERM_ITERLIMIT,
    TimeLimit = BIYOS_TERM_TIMELIMIT,
    CpuTimeLimit = BIYOS_TERM_CPUTIMELIMIT,
    EvaluationLimit = BIYOS_TERM_EVALLIMIT,
    UserStop = BIYOS_TERM_USERSTOP,
    Infeasible = BIYOS_TERM_INFEASIBLE,
    LocallyInfeasible = BIYOS_TERM_LOCALLYINFEASIBLE,
    Unbounded = BIYOS_TERM_UNBOUNDED,
    SlowProgress = BIYOS_TERM_SLOWPROGRESS,
    NumericalTrouble = BIYOS_TERM_NUMERICALTROUBLE,
};

enum class ResultStatus
{
    None = BIYOS_RES_NONE,
    Feasible = BIYOS_RES_FEASIBLE,
    NearlyFeasible = BIYOS_RES_NEARLYFEASIBLE,
    Infeasible = BIYOS_RES_INFEASIBLE,
    Certificate = BIYOS_RES_CERTIFICATE,
    NearlyCertificate = BIYOS_RES_NEARLYCERTIFICATE,
    Unknown = BIYOS_RES_UNKNOWN,
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
