#pragma once

#include "biyos/biyos.h"
#include "core/version.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace biyos
{

class Events;

enum class Option
{
    Seed = BIYOS_OPT_SEED,
    TimeLimit = BIYOS_OPT_MAXTIME,
    CheckLevel = BIYOS_OPT_CHECKLVL,
    Presolve = BIYOS_OPT_PRESOLVE,
    Reproducer = BIYOS_OPT_REPRO,
    FeasibilityAbs = BIYOS_OPT_FEASTOLABS,
    FeasibilityRel = BIYOS_OPT_FEASTOLREL,
    IntegralityTol = BIYOS_OPT_INTTOL,
    ZeroTol = BIYOS_OPT_ZEROTOL,
    CpuTimeLimit = BIYOS_OPT_MAXCPUTIME,
};

enum class OptionKind
{
    Bool = BIYOS_KND_BOOL,
    Int = BIYOS_KND_INT,
    Double = BIYOS_KND_DBL,
    String = BIYOS_KND_STR,
    Enum = BIYOS_KND_ENUM,
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

struct Versions
{
    Version since;
    std::optional<Version> deprecated = std::nullopt;
    std::optional<Version> retired = std::nullopt;
};

struct OptionSpec
{
    Option id;
    std::string_view name;
    std::variant<BoolRule, IntRule, DoubleRule, StringRule, EnumRule> rule;
    std::string_view doc;
    Versions versions;
    std::optional<Option> replacement = std::nullopt;
};

Option find_opt(std::string_view name);
Option find_opt(int id);
std::string_view name_of(Option opt);
OptionKind kind_of(Option opt);
Versions versions_of(Option opt);

class Options
{
  public:
    explicit Options(Events &evts);
    Options(std::span<const OptionSpec> table, Events &evts);

    void set_bool(Option opt, bool value);
    void set_int(Option opt, int value);
    void set_dbl(Option opt, double value);
    void set_str(Option opt, std::string_view value);

    bool get_bool(Option opt) const;
    int get_int(Option opt) const;
    double get_dbl(Option opt) const;
    const std::string &get_str(Option opt) const;

    template <typename E>
        requires std::is_enum_v<E>
    E get_enum(Option opt) const
    {
        return static_cast<E>(position_of(opt));
    }

  private:
    using Value = std::variant<bool, int, double, std::string>;

    static std::vector<Value> defaults(std::span<const OptionSpec> table);

    void store(const OptionSpec &spec, Value value);

    std::size_t position_of(Option opt) const;

    std::span<const OptionSpec> table_;
    Events &events_;
    std::vector<Value> values_;
};

}
