#include "io/native/writers/attributes.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/native/parts.hpp"
#include "io/native/utils/escape.hpp"
#include "model/model.hpp"

#include <cmath>
#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::native
{

namespace
{

bool tolerated(const Tolerance &tolerance) noexcept
{
    return !std::isnan(tolerance.absolute) || !std::isnan(tolerance.relative);
}

std::string tolerance_text(Real value)
{
    return std::isnan(value) ? std::string(kUnset) : text_of(value);
}

template <typename Test>
std::vector<Index> indices_where(const Model &model, Family family, Test test)
{
    std::vector<Index> indices;
    for (Index k = 0; k < model.n_of(family); ++k)
    {
        if (test(k))
        {
            indices.push_back(k);
        }
    }
    return indices;
}

}

void print_name(const Model &model, std::ostream &output)
{
    const std::string *name = model.name();
    if (name != nullptr)
    {
        output << word_of(Part::Name) << ' ' << escaped(*name) << '\n';
    }
}

void print_names(const Model &model, std::ostream &output, Family family)
{
    const auto named = [&](Index k) { return model.name_of(family, k) != nullptr; };
    const std::vector<Index> indices = indices_where(model, family, named);
    if (indices.empty())
    {
        return;
    }
    print_header(output, Part::Names, noun_of(family), indices.size());
    for (const Index k : indices)
    {
        output << k << ' ' << escaped(*model.name_of(family, k)) << '\n';
    }
}

void print_tolerances(const Model &model, std::ostream &output, Family family)
{
    const auto set = [&](Index k) { return tolerated(model.tolerance_of(family, k)); };
    const std::vector<Index> indices = indices_where(model, family, set);
    if (indices.empty())
    {
        return;
    }
    print_header(output, Part::Tolerances, noun_of(family), indices.size());
    for (const Index k : indices)
    {
        const Tolerance tolerance = model.tolerance_of(family, k);
        output << k << ' ' << tolerance_text(tolerance.absolute) << ' '
               << tolerance_text(tolerance.relative) << '\n';
    }
}

}
