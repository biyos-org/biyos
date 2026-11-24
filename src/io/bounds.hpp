#pragma once

#include "core/env.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/vectors.hpp"

namespace biyos
{

void read_bound(Env &env, const Lines &lines, Vectors &vectors, Draft &draft);
void warn_negative(Env &env, const Lines &lines, const Draft &draft);

}
