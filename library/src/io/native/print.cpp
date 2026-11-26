#include "io/native/print.hpp"

#include "core/error.hpp"
#include "io/native/parts.hpp"
#include "io/native/writers/attributes.hpp"
#include "io/native/writers/functions.hpp"
#include "io/native/writers/records.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <locale>
#include <ostream>

namespace biyos
{

void check_native(const Model &model)
{
    if (model.n_callbacks() > 0)
    {
        throw Error(ErrorKind::Unsupported,
                    item("callback", 0),
                    "the native format cannot hold callbacks");
    }
}

void print_native(const Model &model, std::ostream &output)
{
    output.imbue(std::locale::classic());
    output << native::kMagic << ' ' << native::kVersion << '\n';
    native::print_name(model, output);
    native::print_variables(model, output);
    native::print_functions(model, output);
    native::print_linear(model, output);
    native::print_quadratic(model, output);
    native::print_piecewise(model, output);
    native::print_expressions(model, output);
    native::print_constraints(model, output);
    native::print_objectives(model, output);
    native::print_indicators(model, output);
    native::print_sets(model, output);
    native::print_cones(model, output);
    native::print_complements(model, output);
    for (const Family family : kFamilies)
    {
        native::print_names(model, output, family);
    }
    for (const Family family : kFamilies)
    {
        native::print_tolerances(model, output, family);
    }
    output << native::word_of(native::Part::End) << '\n';
}

}
