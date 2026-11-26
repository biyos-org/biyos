#pragma once

#include "core/events/stream.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/mps/vectors.hpp"

namespace biyos
{

void read_bound(Stream stream, const Lines &lines, Vectors &vectors, Draft &draft);
void warn_negative(Stream stream, const Lines &lines, const Draft &draft);

}
