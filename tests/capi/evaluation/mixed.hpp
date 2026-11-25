#pragma once

#include "biyos/biyos.h"

#include <vector>

namespace biyos::test
{

inline const std::vector<double> kPoint{0.3, -0.7, 1.1, 0.8};
inline const std::vector<double> kWeights{0.7, -1.3, 2.1};
inline const std::vector<double> kDirection{0.4, -0.9, 1.7, 0.6};

void build_mixed(BIYOS_model *model, bool product);

}
