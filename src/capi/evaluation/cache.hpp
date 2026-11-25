#pragma once

#include "biyos/biyos.h"
#include "model/evaluation/evaluation.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(BIYOS_model &model);

}
