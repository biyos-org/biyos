#include "core/option_table.hpp"

#include "core/error.hpp"
#include "core/option_checks.hpp"
#include "core/option_text.hpp"
#include "core/version.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr Version kProject{BIYOS_VERSION_MAJOR, BIYOS_VERSION_MINOR, BIYOS_VERSION_PATCH};

constexpr std::array<std::string_view, 3> kCheckLevels{"none", "cheap", "full"};
static_assert(kCheckLevels.size() == static_cast<std::size_t>(CheckLevel::Full) + 1);

constexpr std::array kTable{
    OptionSpec{
        .id = Option::Seed,
        .name = "seed",
        .rule = IntRule{.default_value = 0, .lower = 0},
        .doc = "Seed of every random choice.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::TimeLimit,
        .name = "time_limit",
        .rule = DoubleRule{.default_value = kInfinity, .lower = 0.0},
        .doc = "Wall time limit in seconds.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::CheckLevel,
        .name = "check_level",
        .rule = EnumRule{.default_value = "cheap", .values = kCheckLevels},
        .doc = "Invariant checks to run.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::Presolve,
        .name = "presolve",
        .rule = BoolRule{.default_value = true},
        .doc = "Simplify the model before solving it.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::Reproducer,
        .name = "reproducer",
        .rule = StringRule{.default_value = ""},
        .doc = "File the solve writes its model, options, seed and version to. Empty for none.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::FeasibilityAbs,
        .name = "feasibility_abs",
        .rule = DoubleRule{.default_value = 1e-6, .lower = 0.0},
        .doc = "Absolute tolerance of a bound or an equality.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::FeasibilityRel,
        .name = "feasibility_rel",
        .rule = DoubleRule{.default_value = 1e-6, .lower = 0.0},
        .doc = "Relative tolerance of a bound or an equality.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::IntegralityTol,
        .name = "integrality_tol",
        .rule = DoubleRule{.default_value = 1e-5, .lower = 0.0},
        .doc = "Largest distance to the nearest integer of a value taken as integral.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::ZeroTol,
        .name = "zero_tol",
        .rule = DoubleRule{.default_value = 1e-9, .lower = 0.0},
        .doc = "Largest magnitude of a value taken as zero.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::CpuTimeLimit,
        .name = "cpu_time_limit",
        .rule = DoubleRule{.default_value = kInfinity, .lower = 0.0},
        .doc = "CPU time limit in seconds, counting every thread of the process.",
        .versions = {.since = {0, 1, 0}},
    },
};

static_assert(well_formed(kTable, kProject));
static_assert(kTable.size() == static_cast<std::size_t>(Option::CpuTimeLimit));

}

std::span<const OptionSpec> option_table()
{
    return kTable;
}

std::size_t index_of(Option option)
{
    return static_cast<std::size_t>(option) - 1;
}

const OptionSpec &row_of(std::span<const OptionSpec> table, Option option)
{
    const std::size_t index = index_of(option);
    if (index >= table.size())
    {
        const std::string id = text_of(static_cast<int>(option));
        throw Error(ErrorKind::Internal, "option " + id, "has no table row");
    }
    return table[index];
}

}
