#include "io/reproducer/reproducer.hpp"

#include "core/env/env.hpp"
#include "core/events/stream.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/text/strings.hpp"
#include "io/native/native.hpp"
#include "io/native/print.hpp"
#include "io/reproducer/header.hpp"
#include "io/utils/lines.hpp"
#include "model/model.hpp"

#include <istream>
#include <locale>
#include <ostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

using reproducer::Assignment;

void apply(Options &options, Settings &settings, const Assignment &assignment, Stream stream)
{
    switch (assignment.source)
    {
    case OptionSource::Model:
        settings.set(assignment.option, assignment.value, stream);
        return;
    case OptionSource::Environment:
        options.set(assignment.option, assignment.value);
        return;
    case OptionSource::Default:
        return;
    }
}

}

void print_reproducer(const Model &model, const Lookup &lookup, std::ostream &output)
{
    output.imbue(std::locale::classic());
    reproducer::print_header(output, lookup);
    print_native(model, output);
}

Model read_reproducer(Env &env, Settings &settings, std::istream &input, const std::string &file)
{
    Lines lines(input, file);
    Strings strings;
    const std::span<const OptionSpec> table = env.options().table();
    const std::vector<Assignment> assignments =
        reproducer::read_header(env.stream(), lines, table, strings);
    Model model = read_native(lines);
    Options next(table, env);
    for (const Assignment &assignment : assignments)
    {
        apply(next, settings, assignment, env.stream());
    }
    env.options().replace(std::move(next));
    return model;
}

}
