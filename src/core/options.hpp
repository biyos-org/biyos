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
    FeasAbs = BIYOS_OPT_FEASTOLABS,
    FeasRel = BIYOS_OPT_FEASTOLREL,
    IntTol = BIYOS_OPT_INTTOL,
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

enum class Scope
{
    Model = 1,
    Environment,
};

enum class Source
{
    Model,
    Environment,
    Default,
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
    Scope scope{};
    std::variant<BoolRule, IntRule, DoubleRule, StringRule, EnumRule> rule;
    std::string_view doc;
    Versions versions;
    std::optional<Option> replacement = std::nullopt;
};

using OptionValue = std::variant<bool, int, double, std::string>;

Option find_option(std::string_view name);
Option find_option(int id);
std::string_view name_of(Option option);
OptionKind kind_of(Option option);
Versions versions_of(Option option);

class Options
{
  public:
    explicit Options(Events &events);
    Options(std::span<const OptionSpec> table, Events &events);

    void set_bool(Option option, bool value);
    void set_int(Option option, int value);
    void set_double(Option option, double value);
    void set_string(Option option, std::string_view value);

    bool get_bool(Option option) const;
    int get_int(Option option) const;
    double get_double(Option option) const;
    const std::string &get_string(Option option) const;

    const OptionValue &value_of(Option option) const;
    Source source_of(Option option) const;

    template <typename E>
        requires std::is_enum_v<E>
    E get_enum(Option option) const
    {
        return static_cast<E>(position(option));
    }

  private:
    static std::vector<OptionValue> defaults(std::span<const OptionSpec> table);

    void store(Option option, OptionValue value);

    std::size_t position(Option option) const;

    std::span<const OptionSpec> table_;
    Events &events_;
    std::vector<OptionValue> values_;
    std::vector<Source> sources_;
};

}
