#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct ColumnTerms
{
    std::vector<Offset> starts;
    std::vector<Index> functions;
    std::vector<Real> values;
};

std::vector<Offset> variable_starts(const Model &model, Index start, Index stop);
void variable_terms(const Model &model,
                    Index start,
                    std::span<const Offset> starts,
                    std::span<Index> functions,
                    std::span<Real> values);
std::size_t column_count(const Model &model, std::span<const Index> variables);
ColumnTerms columns_of(const Model &model, std::span<const Index> variables);
ColumnTerms columns_of(const Model &model, Index start, Index stop);

}
