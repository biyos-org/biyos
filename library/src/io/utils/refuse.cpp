#include "io/utils/refuse.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/text/text.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <string>
#include <string_view>

namespace biyos
{

void refuse_part(std::string_view format, std::string_view noun, Index index, std::string_view part)
{
    const std::string reason = std::string(format) + " cannot hold " + std::string(part);
    throw Error(ErrorKind::Unsupported, item(noun, index), reason);
}

void check_absent(std::string_view format, const Model &model, Family family, std::string_view part)
{
    if (model.n_of(family) > 0)
    {
        refuse_part(format, noun_of(family), 0, part);
    }
}

void check_weight(std::string_view format, const Model &model, Index objective)
{
    const Real weight = model.objective(objective).weight;
    if (!identical(weight, 1.0))
    {
        refuse_part(format,
                    "objective",
                    objective,
                    "a weight other than 1, here " + text_of(weight));
    }
}

}
