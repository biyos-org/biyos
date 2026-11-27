#pragma once

#include "core/events/stream.hpp"
#include "io/draft/draft.hpp"
#include "io/mps/readers/vectors.hpp"
#include "io/utils/lines.hpp"

namespace biyos
{

void read_rhs(Stream stream, const Lines &lines, Vectors &vectors, Draft &draft);
void read_range(Stream stream, const Lines &lines, Vectors &vectors, Draft &draft);

}
