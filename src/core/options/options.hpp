#pragma once

#include "biyos/biyos.h"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "core/text/strings.hpp"
#include "core/version.hpp"

#include <cstdint>
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

class Env;

enum class Option
{
    Seed = BIYOS_OPT_SEED,
    TimeLimit = BIYOS_OPT_MAXTIME,
    Checking = BIYOS_OPT_CHK,
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
    Bool = BIYOS_OPTKND_BOOL,
    Int = BIYOS_OPTKND_INT,
    Double = BIYOS_OPTKND_DBL,
    String = BIYOS_OPTKND_STR,
    Enum = BIYOS_OPTKND_ENUM,
    Flags = BIYOS_OPTKND_FLG,
};

enum class Scope
{
    Model = 1,
    Environment,
};

enum class OptionSource
{
    Model = BIYOS_OPTSRC_MDL,
    Environment = BIYOS_OPTSRC_ENV,
    Default = BIYOS_OPTSRC_DEFAULT,
};

enum class Checking
{
    None = BIYOS_CHK_NONE,
    Cheap = BIYOS_CHK_CHEAP,
    Full = BIYOS_CHK_FULL,
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
    Real default_value;
    Real lower = -kInfinity;
    Real upper = kInfinity;
};

struct StringRule
{
    std::string_view default_value;
};

struct EnumValue
{
    int number;
    std::string_view name;
};

struct EnumRule
{
    int default_value;
    std::span<const EnumValue> values;
};

using Mask = std::uint64_t;

enum class PartDefault
{
    On,
    Off,
    Automatic,
};

enum class PartState
{
    On,
    Off,
    Default,
};

struct FlagPart
{
    Mask bit;
    std::string_view name;
    PartDefault default_value;
};

struct FlagsRule
{
    std::span<const FlagPart> parts;
};

struct Masks
{
    Mask on = 0;
    Mask off = 0;

    bool operator==(const Masks &) const = default;
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
    std::variant<BoolRule, IntRule, DoubleRule, StringRule, EnumRule, FlagsRule> rule;
    std::string_view doc;
    Versions versions;
    std::optional<Option> replacement = std::nullopt;
};

using OptionValue = std::variant<bool, int, Real, std::string_view, Masks>;

Option find_option(std::string_view name);
Option find_option(int id);
std::string_view name_of(Option option);
OptionKind kind_of(Option option);
Versions versions_of(Option option);

class Options
{
  public:
    explicit Options(Env &env);
    Options(std::span<const OptionSpec> table, Env &env);

    void set_bool(Option option, bool value);
    void set_int(Option option, int value);
    void set_double(Option option, Real value);
    void set_string(Option option, std::string_view value);
    void set_flags(Option option, Masks value);

    bool get_bool(Option option) const;
    int get_int(Option option) const;
    Real get_double(Option option) const;
    std::string_view get_string(Option option) const;
    Masks get_flags(Option option) const;

    const OptionValue &value_of(Option option) const;
    OptionSource source_of(Option option) const;
    std::span<const OptionSpec> table() const noexcept;

    template <typename E>
        requires std::is_enum_v<E>
    E get_enum(Option option) const
    {
        return static_cast<E>(enum_value(option));
    }

  private:
    std::vector<OptionValue> defaults();

    void store(Option option, const OptionValue &value);

    int enum_value(Option option) const;

    std::span<const OptionSpec> table_;
    Env &env_;
    Strings strings_;
    std::vector<OptionValue> values_;
    std::vector<OptionSource> sources_;
};

}
