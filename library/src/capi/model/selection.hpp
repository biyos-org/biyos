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

std::span<const Index> selected(Family family, Index total, List list);
std::vector<Index> selected(Family family, Index total, Range range);
std::span<const Index> selected(std::string_view noun,
                                std::string_view word,
                                Index total,
                                List list);
std::vector<Index> selected(std::string_view noun, std::string_view word, Index total, Range range);
void check_known(Family family, Index total, std::span<const Index> indices);
void check_known(std::string_view noun, Index total, std::span<const Index> indices);

}
