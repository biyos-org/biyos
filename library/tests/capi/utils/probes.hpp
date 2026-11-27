#pragma once

#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::test
{

inline void mark(const BIYOS_event *event, void *data)
{
    static_cast<std::vector<const BIYOS_model *> *>(data)->push_back(BIYOS_getevtmdl(event));
}

inline std::string written(std::string_view name, std::string_view text)
{
    const std::string path = testing::TempDir() + std::string(name);
    std::ofstream(path) << text;
    return path;
}

}
