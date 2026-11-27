#pragma once

#include "core/options/options.hpp"
#include "core/text/strings.hpp"
#include "io/native/utils/record.hpp"

#include <ostream>

namespace biyos::reproducer
{

void print_value(std::ostream &output, const OptionSpec &spec, const OptionValue &value);
OptionValue read_value(native::Record &record, const OptionSpec &spec, Strings &strings);

}
