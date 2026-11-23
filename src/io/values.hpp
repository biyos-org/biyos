#pragma once

#include "core/env.hpp"
#include "io/draft.hpp"
#include "io/lines.hpp"
#include "io/vectors.hpp"

namespace biyos
{

void read_rhs(Env &env, const Lines &lines, Vectors &vectors, Draft &draft);
void read_range(Env &env, const Lines &lines, Vectors &vectors, Draft &draft);

}
