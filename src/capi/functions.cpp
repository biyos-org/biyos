#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

std::vector<double> constants_of(std::size_t count, const double *constants)
{
    if (constants == nullptr)
    {
        return std::vector<double>(count, 0.0);
    }
    return std::vector<double>(constants, constants + count);
}

std::vector<biyos::Offset> offsets(std::size_t count, const std::int64_t *starts)
{
    if (starts == nullptr)
    {
        return std::vector<biyos::Offset>(count + 1, 0);
    }
    return std::vector<biyos::Offset>(starts, starts + count + 1);
}

void add_functions(biyos_model &model,
                   int count,
                   const double *constants,
                   const std::int64_t *starts,
                   const int *variables,
                   const double *values,
                   int *first)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(count);
    const std::vector<double> added = constants_of(size, constants);
    const std::vector<biyos::Offset> begins = offsets(size, starts);
    biyos::check_starts(begins);
    core.check_room(size, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    const biyos::Index start = core.function_count();
    core.add_functions(added, begins, biyos::capi::packed(total, variables, values));
    if (first != nullptr)
    {
        *first = start;
    }
}

void function_count(biyos_model &model, int *count)
{
    *require(count, "count") = model.core.function_count();
}

void write_constants(std::span<const double> found, double *constants)
{
    if (constants != nullptr)
    {
        std::ranges::copy(found, constants);
    }
}

void write_starts(const biyos::Model &core, int first, std::size_t count, std::int64_t *starts)
{
    if (starts == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j <= count; ++j)
    {
        const std::size_t before = core.terms(first, static_cast<biyos::Index>(j)).size();
        starts[j] = static_cast<std::int64_t>(before);
    }
}

void get_functions(biyos_model &model,
                   int first,
                   int count,
                   double *constants,
                   std::int64_t *starts,
                   std::int64_t size,
                   std::int64_t *needed,
                   int *variables,
                   double *values)
{
    const std::span<const double> held = model.core.constants(first, count);
    const std::span<const biyos::LinearTerm> found = model.core.terms(first, count);
    const std::size_t room = length(size, "size");
    require(needed, "needed");
    write_constants(held, constants);
    write_starts(model.core, first, held.size(), starts);
    biyos::capi::unpack(found, room, variables, values);
    *needed = static_cast<std::int64_t>(found.size());
}

}

biyos_status biyos_add_functions(biyos_model *model,
                                 int count,
                                 const double *constants,
                                 const int64_t *starts,
                                 const int *variables,
                                 const double *values,
                                 int *first)
{
    return guarded<add_functions>(model, count, constants, starts, variables, values, first);
}

biyos_status biyos_function_count(biyos_model *model, int *count)
{
    return guarded<function_count>(model, count);
}

biyos_status biyos_get_functions(biyos_model *model,
                                 int first,
                                 int count,
                                 double *constants,
                                 int64_t *starts,
                                 int64_t size,
                                 int64_t *needed,
                                 int *variables,
                                 double *values)
{
    return guarded<get_functions>(model,
                                  first,
                                  count,
                                  constants,
                                  starts,
                                  size,
                                  needed,
                                  variables,
                                  values);
}
