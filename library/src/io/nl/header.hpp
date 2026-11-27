#pragma once

#include "core/numbers.hpp"
#include "io/utils/lines.hpp"
#include "model/model.hpp"

#include <vector>

namespace biyos::nl
{

enum class Encoding
{
    Text,
    Little,
    Big,
};

struct Header
{
    Index variables;
    Index constraints;
    Index objectives;
    Index defined;
    std::vector<VariableKind> kinds;
    Encoding encoding;
};

Header read_header(Lines &lines, Offset bytes);

}
