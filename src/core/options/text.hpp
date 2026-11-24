#pragma once

#include "core/options/options.hpp"
#include "core/version.hpp"

#include <span>
#include <string>
#include <string_view>

namespace biyos
{

std::string subject(std::string_view name);
std::string joined(std::span<const EnumValue> values);
std::string joined(std::span<const FlagPart> parts);
std::string bits(Mask mask);
std::string_view noun(const BoolRule &);
std::string_view noun(const IntRule &);
std::string_view noun(const DoubleRule &);
std::string_view noun(const StringRule &);
std::string_view noun(const EnumRule &);
std::string_view noun(const FlagsRule &);
std::string_view noun(const OptionSpec &spec);
std::string dotted(Version version);
std::string notice(std::string_view stage, Version version, std::string_view replacement);

}
