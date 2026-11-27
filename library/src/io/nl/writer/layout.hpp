#pragma once

#include "core/numbers.hpp"
#include "io/nl/writer/usage.hpp"
#include "model/model.hpp"

#include <vector>

namespace biyos::nl
{

struct Records
{
    std::vector<Row> rows;
    Index nonlinear_rows;
    Index nonlinear_complements;
    Index nonlinear_objectives;
};

struct Counts
{
    Index in_constraints;
    Index in_objectives;
    Index in_both;
    Index binaries;
    Index integers;
    Index both_integers;
    Index constraint_integers;
    Index objective_integers;
};

struct Columns
{
    std::vector<Index> order;
    std::vector<Index> position;
    Counts counts;
    bool integral_binaries;
};

bool nonlinear(const Model &model, Index function);
Index variable_of(const Model &model, const Complement &complement);
Records records_of(const Model &model);
Columns columns_of(const Model &model, const Usage &usage);

}
