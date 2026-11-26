#pragma once

#include "core/numbers.hpp"
#include "io/lines.hpp"
#include "model/model.hpp"

#include <vector>

namespace biyos::nl
{

struct Header
{
    Index variables;
    Index constraints;
    Index objectives;
    Index defined;
    std::vector<VariableKind> kinds;
};

Header read_header(Lines &lines, Offset bytes);

}
