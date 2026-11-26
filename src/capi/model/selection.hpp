#pragma once

#include "biyos/biyos.h"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace biyos::capi
{

struct List
{
    BIYOS_index n;
    const BIYOS_index *indices;
};

struct Range
{
    BIYOS_index start;
    BIYOS_index stop;
};

std::span<const Index> selected(const Model &model, Family family, List list);
std::vector<Index> selected(const Model &model, Family family, Range range);
std::span<const Index> selected(std::string_view noun, Index total, List list);
std::vector<Index> selected(std::string_view noun, Index total, Range range);
void check_known(const Model &model, Family family, std::span<const Index> indices);

}
