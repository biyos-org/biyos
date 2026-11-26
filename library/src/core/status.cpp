#include "core/status.hpp"

namespace biyos
{

std::string_view name_of(TerminationStatus status) noexcept
{
    switch (status)
    {
    case TerminationStatus::NotSolved:
        return "not solved";
    case TerminationStatus::Optimal:
        return "optimal";
    case TerminationStatus::LocallySolved:
        return "locally solved";
    case TerminationStatus::AlmostLocallySolved:
        return "almost locally solved";
    case TerminationStatus::IterationLimit:
        return "iteration limit";
    case TerminationStatus::TimeLimit:
        return "time limit";
    case TerminationStatus::CpuTimeLimit:
        return "CPU time limit";
    case TerminationStatus::EvaluationLimit:
        return "evaluation limit";
    case TerminationStatus::UserStop:
        return "user stop";
    case TerminationStatus::Infeasible:
        return "infeasible";
    case TerminationStatus::LocallyInfeasible:
        return "locally infeasible";
    case TerminationStatus::Unbounded:
        return "unbounded";
    case TerminationStatus::SlowProgress:
        return "slow progress";
    case TerminationStatus::NumericalTrouble:
        return "numerical trouble";
    }
    return "";
}

std::string_view name_of(ResultStatus status) noexcept
{
    switch (status)
    {
    case ResultStatus::None:
        return "none";
    case ResultStatus::Feasible:
        return "feasible";
    case ResultStatus::NearlyFeasible:
        return "nearly feasible";
    case ResultStatus::Infeasible:
        return "infeasible";
    case ResultStatus::Certificate:
        return "certificate";
    case ResultStatus::NearlyCertificate:
        return "nearly certificate";
    case ResultStatus::Unknown:
        return "unknown";
    }
    return "";
}

std::string_view name_of(Claim claim) noexcept
{
    switch (claim)
    {
    case Claim::None:
        return "none";
    case Claim::Local:
        return "local";
    case Claim::Global:
        return "global";
    }
    return "";
}

}
