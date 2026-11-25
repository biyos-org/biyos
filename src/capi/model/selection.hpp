#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

namespace biyos::capi
{

struct List
{
    int n;
    const int *indices;
};

struct Range
{
    int start;
    int stop;
};

std::span<const Index> selected(const Model &model, Family family, List list);
std::vector<Index> selected(const Model &model, Family family, Range range);

}
