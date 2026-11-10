#pragma once

#include "biyos/options.h"

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace biyos
{

enum class Option
{
    Seed = BIYOS_OPT_SEED,
    TimeLimit = BIYOS_OPT_TIME_LIMIT,
    CheckLevel = BIYOS_OPT_CHECK_LEVEL,
    Presolve = BIYOS_OPT_PRESOLVE,
    Reproducer = BIYOS_OPT_REPRODUCER,
    FeasibilityAbs = BIYOS_OPT_FEASIBILITY_ABS,
    FeasibilityRel = BIYOS_OPT_FEASIBILITY_REL,
    IntegralityTol = BIYOS_OPT_INTEGRALITY_TOL,
    ZeroTol = BIYOS_OPT_ZERO_TOL,
    CpuTimeLimit = BIYOS_OPT_CPU_TIME_LIMIT,
};

enum class OptionKind
{
    Bool = BIYOS_KIND_BOOL,
    Int = BIYOS_KIND_INT,
    Double = BIYOS_KIND_DOUBLE,
    String = BIYOS_KIND_STRING,
    Enum = BIYOS_KIND_ENUM,
};

enum class CheckLevel
{
    None,
    Cheap,
    Full,
};

struct BoolRule
{
    bool default_value;
};

struct IntRule
{
    int default_value;
    int lower = (std::numeric_limits<int>::min)();
    int upper = (std::numeric_limits<int>::max)();
};

struct DoubleRule
{
    double default_value;
    double lower = -std::numeric_limits<double>::infinity();
    double upper = std::numeric_limits<double>::infinity();
};

struct StringRule
{
    std::string_view default_value;
};

struct EnumRule
{
    std::string_view default_value;
    std::span<const std::string_view> values;
};

struct OptionSpec
{
    Option id;
    std::string_view name;
    std::variant<BoolRule, IntRule, DoubleRule, StringRule, EnumRule> rule;
    std::string_view doc;
};

Option find_option(std::string_view name);
Option find_option(int id);
std::string_view name_of(Option option);
OptionKind kind_of(Option option);

// A value for each row of an option table, which outlives the store unchanged. The built-in table
// is checked at compile time, a given one when the store is built.
class Options
{
  public:
    Options();
    explicit Options(std::span<const OptionSpec> table);

    void set_bool(Option option, bool value);
    void set_int(Option option, int value);
    void set_double(Option option, double value);
    void set_string(Option option, std::string_view value);

    bool get_bool(Option option) const;
    int get_int(Option option) const;
    double get_double(Option option) const;
    const std::string &get_string(Option option) const;

    template <typename E>
        requires std::is_enum_v<E>
    E get_enum(Option option) const
    {
        return static_cast<E>(position_of(option));
    }

  private:
    using Value = std::variant<bool, int, double, std::string>;

    static std::vector<Value> defaults(std::span<const OptionSpec> table);

    std::size_t position_of(Option option) const;

    std::span<const OptionSpec> table_;
    std::vector<Value> values_;
};

}
