#pragma once

#include "biyos/biyos.h"

#include <vector>

namespace biyos::test
{

inline void mark(const BIYOS_event *event, void *data)
{
    static_cast<std::vector<const BIYOS_model *> *>(data)->push_back(BIYOS_getevtmdl(event));
}

}
