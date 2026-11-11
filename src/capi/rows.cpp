#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr biyos::Row kFree{.lower = -kInfinity, .upper = kInfinity};

biyos::Row bounded(biyos::Row row, std::size_t k, const double *lower, const double *upper)
{
    if (lower != nullptr)
    {
        row.lower = lower[k];
    }
    if (upper != nullptr)
    {
        row.upper = upper[k];
    }
    return row;
}

std::vector<biyos::Offset> offsets(std::size_t count, const std::int64_t *starts)
{
    if (starts == nullptr)
    {
        return std::vector<biyos::Offset>(count + 1, 0);
    }
    return std::vector<biyos::Offset>(starts, starts + count + 1);
}

void add_rows(biyos_model &model, int count, const double *lower, const double *upper,
              const std::int64_t *starts, const int *columns, const double *values)
{
    biyos::Model &core = writable(model);
    const std::size_t size = length(count);
    std::vector<biyos::Row> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        added.push_back(bounded(kFree, k, lower, upper));
    }
    const std::vector<biyos::Offset> begins = offsets(size, starts);
    biyos::check_starts(begins);
    core.check_room(size, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    core.add_rows(added, begins, biyos::capi::packed(total, columns, values));
}

void set_bounds(biyos_model &model, int count, const int *indices, const double *lower,
                const double *upper)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> changed = listed(count, indices);
    std::vector<biyos::Row> rows;
    rows.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        rows.push_back(bounded(core.row(changed[k]), k, lower, upper));
    }
    core.set_rows(changed, rows);
}

void row_count(biyos_model &model, int *count)
{
    *require(count, "count") = model.core.row_count();
}

void get_rows(biyos_model &model, int first, int count, double *lower, double *upper)
{
    const std::span<const biyos::Row> found = model.core.rows(first, count);
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        if (lower != nullptr)
        {
            lower[k] = found[k].lower;
        }
        if (upper != nullptr)
        {
            upper[k] = found[k].upper;
        }
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
        const std::size_t before = core.entries(first, static_cast<biyos::Index>(j)).size();
        starts[j] = static_cast<std::int64_t>(before);
    }
}

void get_entries(biyos_model &model, int first, int count, std::int64_t *starts, std::int64_t size,
                 std::int64_t *needed, int *columns, double *values)
{
    const std::span<const biyos::Entry> found = model.core.entries(first, count);
    const std::size_t room = length(size, "size");
    require(needed, "needed");
    write_starts(model.core, first, static_cast<std::size_t>(count), starts);
    biyos::capi::unpack(found, room, columns, values);
    *needed = static_cast<std::int64_t>(found.size());
}

}

biyos_status biyos_add_rows(biyos_model *model, int count, const double *lower, const double *upper,
                            const int64_t *starts, const int *columns, const double *values)
{
    return guarded<add_rows>(model, count, lower, upper, starts, columns, values);
}

biyos_status biyos_set_row_bounds(biyos_model *model, int count, const int *indices,
                                  const double *lower, const double *upper)
{
    return guarded<set_bounds>(model, count, indices, lower, upper);
}

biyos_status biyos_row_count(biyos_model *model, int *count)
{
    return guarded<row_count>(model, count);
}

biyos_status biyos_get_rows(biyos_model *model, int first, int count, double *lower, double *upper)
{
    return guarded<get_rows>(model, first, count, lower, upper);
}

biyos_status biyos_get_entries(biyos_model *model, int first, int count, int64_t *starts,
                               int64_t size, int64_t *needed, int *columns, double *values)
{
    return guarded<get_entries>(model, first, count, starts, size, needed, columns, values);
}
