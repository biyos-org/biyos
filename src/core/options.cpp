#include "core/options.hpp"

#include "core/error.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <span>

namespace biyos
{

namespace
{

constexpr int kIntMin = (std::numeric_limits<int>::min)();
constexpr int kIntMax = (std::numeric_limits<int>::max)();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct BoolRule
{
    bool default_value;
};

struct IntRule
{
    int default_value;
    int lower = kIntMin;
    int upper = kIntMax;
};

struct DoubleRule
{
    double default_value;
    double lower = -kInfinity;
    double upper = kInfinity;
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

constexpr std::array<std::string_view, 3> kCheckLevels{"none", "cheap", "full"};

constexpr std::array kTable{
    OptionSpec{
        .id = Option::Seed,
        .name = "seed",
        .rule = IntRule{.default_value = 0, .lower = 0},
        .doc = "Seed of every random choice.",
    },
    OptionSpec{
        .id = Option::TimeLimit,
        .name = "time_limit",
        .rule = DoubleRule{.default_value = kInfinity, .lower = 0.0},
        .doc = "Wall time limit in seconds.",
    },
    OptionSpec{
        .id = Option::CheckLevel,
        .name = "check_level",
        .rule = EnumRule{.default_value = "cheap", .values = kCheckLevels},
        .doc = "Invariant checks to run.",
    },
    OptionSpec{
        .id = Option::Presolve,
        .name = "presolve",
        .rule = BoolRule{.default_value = true},
        .doc = "Simplify the model before solving it.",
    },
    OptionSpec{
        .id = Option::Reproducer,
        .name = "reproducer",
        .rule = StringRule{.default_value = ""},
        .doc = "File the solve writes its model, options, seed and version to. Empty for none.",
    },
};

constexpr bool admits(const BoolRule &)
{
    return true;
}

constexpr bool admits(const IntRule &rule)
{
    return rule.lower <= rule.default_value && rule.default_value <= rule.upper;
}

constexpr bool admits(const DoubleRule &rule)
{
    return rule.lower <= rule.default_value && rule.default_value <= rule.upper;
}

constexpr bool admits(const StringRule &)
{
    return true;
}

constexpr bool admits(const EnumRule &rule)
{
    return std::ranges::find(rule.values, rule.default_value) != rule.values.end();
}

constexpr bool admits(const OptionSpec &spec)
{
    return std::visit([](const auto &rule) { return admits(rule); }, spec.rule);
}

constexpr bool well_formed()
{
    for (std::size_t i = 0; i < kTable.size(); ++i)
    {
        const OptionSpec &spec = kTable[i];
        const bool in_order = static_cast<std::size_t>(spec.id) == i + 1;
        const bool unique = std::ranges::count(kTable, spec.name, &OptionSpec::name) == 1;
        if (!in_order || !unique || !admits(spec))
        {
            return false;
        }
    }
    return true;
}

static_assert(well_formed());
static_assert(kTable.size() == static_cast<std::size_t>(Option::Reproducer));

constexpr std::string_view noun(const BoolRule &)
{
    return "a bool";
}

constexpr std::string_view noun(const IntRule &)
{
    return "an int";
}

constexpr std::string_view noun(const DoubleRule &)
{
    return "a double";
}

constexpr std::string_view noun(const StringRule &)
{
    return "a string";
}

constexpr std::string_view noun(const EnumRule &)
{
    return "an enum";
}

constexpr std::string_view noun(const OptionSpec &spec)
{
    return std::visit([](const auto &rule) { return noun(rule); }, spec.rule);
}

bool default_of(const BoolRule &rule)
{
    return rule.default_value;
}

int default_of(const IntRule &rule)
{
    return rule.default_value;
}

double default_of(const DoubleRule &rule)
{
    return rule.default_value;
}

std::string default_of(const StringRule &rule)
{
    return std::string(rule.default_value);
}

std::string default_of(const EnumRule &rule)
{
    return std::string(rule.default_value);
}

template <typename T> std::string text_of(T value)
{
    std::array<char, 32> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

std::size_t index_of(Option option)
{
    return static_cast<std::size_t>(option) - 1;
}

const OptionSpec &spec_of(Option option)
{
    const std::size_t index = index_of(option);
    if (index >= kTable.size())
    {
        const std::string id = text_of(static_cast<int>(option));
        throw Error(ErrorKind::Internal, "option " + id, "has no table row");
    }
    return kTable[index];
}

std::string subject(std::string_view name)
{
    return "option '" + std::string(name) + "'";
}

template <typename Rule> const Rule &rule_of(const OptionSpec &spec)
{
    if (const Rule *rule = std::get_if<Rule>(&spec.rule))
    {
        return *rule;
    }
    const std::string held(noun(spec));
    const std::string wanted(noun(Rule{}));
    throw Error(ErrorKind::InvalidArgument, subject(spec.name), "is " + held + ", not " + wanted);
}

template <typename Rule, typename T>
void check_range(const OptionSpec &spec, const Rule &rule, T value)
{
    if (rule.lower <= value && value <= rule.upper)
    {
        return;
    }
    const std::string range = "[" + text_of(rule.lower) + ", " + text_of(rule.upper) + "]";
    throw Error(ErrorKind::InvalidArgument, subject(spec.name),
                text_of(value) + " is outside " + range);
}

std::string joined(std::span<const std::string_view> names)
{
    std::string text;
    for (const std::string_view name : names)
    {
        if (!text.empty())
        {
            text += ", ";
        }
        text += name;
    }
    return text;
}

void check_value(const OptionSpec &spec, const EnumRule &rule, std::string_view value)
{
    if (std::ranges::find(rule.values, value) != rule.values.end())
    {
        return;
    }
    throw Error(ErrorKind::InvalidArgument, subject(spec.name),
                "'" + std::string(value) + "' is not one of " + joined(rule.values));
}

}

Option find_option(std::string_view name)
{
    const auto spec = std::ranges::find(kTable, name, &OptionSpec::name);
    if (spec == kTable.end())
    {
        throw Error(ErrorKind::InvalidArgument, subject(name), "unknown");
    }
    return spec->id;
}

Option find_option(int id)
{
    if (id < 1 || static_cast<std::size_t>(id) > kTable.size())
    {
        throw Error(ErrorKind::InvalidArgument, "option " + text_of(id), "unknown");
    }
    return static_cast<Option>(id);
}

std::string_view name_of(Option option)
{
    return spec_of(option).name;
}

Options::Options()
{
    values_.reserve(kTable.size());
    const auto value_of = [](const auto &rule) { return Value(default_of(rule)); };
    for (const OptionSpec &spec : kTable)
    {
        values_.push_back(std::visit(value_of, spec.rule));
    }
}

void Options::set_bool(Option option, bool value)
{
    rule_of<BoolRule>(spec_of(option));
    std::get<bool>(values_[index_of(option)]) = value;
}

void Options::set_int(Option option, int value)
{
    const OptionSpec &spec = spec_of(option);
    check_range(spec, rule_of<IntRule>(spec), value);
    std::get<int>(values_[index_of(option)]) = value;
}

void Options::set_double(Option option, double value)
{
    const OptionSpec &spec = spec_of(option);
    check_range(spec, rule_of<DoubleRule>(spec), value);
    std::get<double>(values_[index_of(option)]) = value;
}

void Options::set_string(Option option, std::string_view value)
{
    const OptionSpec &spec = spec_of(option);
    if (const EnumRule *rule = std::get_if<EnumRule>(&spec.rule))
    {
        check_value(spec, *rule, value);
    }
    else
    {
        rule_of<StringRule>(spec);
    }
    std::get<std::string>(values_[index_of(option)]).assign(value);
}

bool Options::get_bool(Option option) const
{
    rule_of<BoolRule>(spec_of(option));
    return std::get<bool>(values_[index_of(option)]);
}

int Options::get_int(Option option) const
{
    rule_of<IntRule>(spec_of(option));
    return std::get<int>(values_[index_of(option)]);
}

double Options::get_double(Option option) const
{
    rule_of<DoubleRule>(spec_of(option));
    return std::get<double>(values_[index_of(option)]);
}

const std::string &Options::get_string(Option option) const
{
    const OptionSpec &spec = spec_of(option);
    if (!std::holds_alternative<EnumRule>(spec.rule))
    {
        rule_of<StringRule>(spec);
    }
    return std::get<std::string>(values_[index_of(option)]);
}

}
