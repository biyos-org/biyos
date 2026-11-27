#pragma once

#include "core/events/stream.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/text/strings.hpp"
#include "io/utils/lines.hpp"

#include <ostream>
#include <span>
#include <vector>

namespace biyos::reproducer
{

struct Assignment
{
    Option option;
    OptionSource source;
    OptionValue value;
};

void print_header(std::ostream &output, const Lookup &lookup);
std::vector<Assignment> read_header(Stream stream,
                                    Lines &lines,
                                    std::span<const OptionSpec> table,
                                    Strings &strings);

}
