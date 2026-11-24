#include "core/options/table.hpp"

#include "core/error.hpp"
#include "core/options/checks.hpp"
#include "core/text/text.hpp"
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

constexpr std::array kChecking{
    EnumValue{.number = BIYOS_CHK_NONE, .name = "none"},
    EnumValue{.number = BIYOS_CHK_CHEAP, .name = "cheap"},
    EnumValue{.number = BIYOS_CHK_FULL, .name = "full"},
};
static_assert(kChecking.size() == static_cast<std::size_t>(Checking::Full));

constexpr std::array kTable{
    OptionSpec{
        .id = Option::Seed,
        .name = "seed",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 0, .lower = 0},
        .doc = "Seed of every random choice.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::TimeLimit,
        .name = "maxtime",
        .scope = Scope::Model,
        .rule = DoubleRule{.default_value = kInfinity, .lower = 0.0},
        .doc = "Wall time limit in seconds.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::Checking,
        .name = "checking",
        .scope = Scope::Model,
        .rule = EnumRule{.default_value = BIYOS_CHK_CHEAP, .values = kChecking},
        .doc = "Invariant checks to run.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::Presolve,
        .name = "presolve",
        .scope = Scope::Model,
        .rule = BoolRule{.default_value = true},
        .doc = "Simplify the model before solving it.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::Reproducer,
        .name = "repro",
        .scope = Scope::Model,
        .rule = StringRule{.default_value = ""},
        .doc = "File the solve writes its model, options, seed and version to. Empty for none.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::FeasAbs,
        .name = "feastol_abs",
        .scope = Scope::Model,
        .rule = DoubleRule{.default_value = 1e-6, .lower = 0.0},
        .doc = "Absolute tolerance of a bound or an equality.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::FeasRel,
        .name = "feastol_rel",
        .scope = Scope::Model,
        .rule = DoubleRule{.default_value = 1e-6, .lower = 0.0},
        .doc = "Relative tolerance of a bound or an equality.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::IntTol,
        .name = "inttol",
        .scope = Scope::Model,
        .rule = DoubleRule{.default_value = 1e-5, .lower = 0.0},
        .doc = "Largest distance to the nearest integer of a value taken as integral.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::ZeroTol,
        .name = "zerotol",
        .scope = Scope::Model,
        .rule = DoubleRule{.default_value = 1e-9, .lower = 0.0},
        .doc = "Largest magnitude of a value taken as zero.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::CpuTimeLimit,
        .name = "maxcputime",
        .scope = Scope::Model,
        .rule = DoubleRule{.default_value = kInfinity, .lower = 0.0},
        .doc = "CPU time limit in seconds, counting the thread that makes the call.",
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
