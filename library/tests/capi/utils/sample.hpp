#pragma once

#include "biyos/biyos.h"

#include <random>

namespace biyos::test
{

using Engine = std::mt19937;

int below(Engine &engine, int n);
void build_sample(BIYOS_model *model, Engine &engine);

}
