#include "model/attributes/records.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/utils/checks.hpp"
#include "model/utils/renumber.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr Real kNan = std::numeric_limits<Real>::quiet_NaN();
constexpr Tolerance kGlobal{.absolute = kNan, .relative = kNan};

void check_size(std::span<const Index> indices, std::span<const Real> values)
{
    if (!values.empty() && values.size() != indices.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and tolerances differ in number");
    }
}

std::string field_of(std::string_view part)
{
    return std::string(part) + " tolerance ";
}

void check_value(std::string_view noun, Index index, std::string_view part, Real value)
{
    const std::string_view fault = value_fault(value);
    if (!fault.empty())
    {
        refuse(noun, index, field_of(part) + std::string(fault));
    }
    if (std::signbit(value) && std::fpclassify(value) != FP_ZERO)
    {
        refuse(noun, index, field_of(part) + text_of(value) + " is negative");
    }
}

void check_entry(std::string_view noun,
                 Index index,
                 std::span<const Real> values,
                 std::size_t k,
                 std::string_view part)
{
    if (!values.empty())
    {
        check_value(noun, index, part, values[k]);
    }
}

void assign(Real &held, std::span<const Real> values, std::size_t k) noexcept
{
    if (!values.empty())
    {
        held = values[k];
    }
}

std::vector<std::size_t> order_of(std::span<const Index> indices)
{
    std::vector<std::size_t> order(indices.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, {}, [indices](std::size_t k) { return std::pair(indices[k], k); });
    return order;
}

struct Batch
{
    std::span<const Index> indices;
    std::span<const Real> absolute;
    std::span<const Real> relative;
    std::vector<std::size_t> order;

    std::size_t size() const noexcept { return indices.size(); }
    std::size_t source(std::size_t next) const noexcept;
    Index index(std::size_t next) const noexcept { return indices[source(next)]; }
    bool shadowed(std::size_t next) const noexcept;
    void apply(std::size_t next, Tolerance &value) const noexcept;
};

std::size_t Batch::source(std::size_t next) const noexcept
{
    return order.empty() ? next : order[next];
}

bool Batch::shadowed(std::size_t next) const noexcept
{
    return next + 1 < size() && index(next + 1) == index(next);
}

void Batch::apply(std::size_t next, Tolerance &value) const noexcept
{
    assign(value.absolute, absolute, source(next));
    assign(value.relative, relative, source(next));
}

void check(std::string_view noun, Index size, const Batch &batch)
{
    check_size(batch.indices, batch.absolute);
    check_size(batch.indices, batch.relative);
    for (std::size_t next = 0; next < batch.size(); ++next)
    {
        const Index index = batch.index(next);
        const std::size_t k = batch.source(next);
        check_index(noun, index, size);
        check_entry(noun, index, batch.absolute, k, "absolute");
        check_entry(noun, index, batch.relative, k, "relative");
    }
}

std::size_t position(std::span<const Index> held, Index index) noexcept
{
    return static_cast<std::size_t>(std::ranges::lower_bound(held, index) - held.begin());
}

bool holds_all(std::span<const Index> held, const Batch &batch) noexcept
{
    for (std::size_t next = 0; next < batch.size(); ++next)
    {
        if (!std::ranges::binary_search(held, batch.index(next)))
        {
            return false;
        }
    }
    return true;
}

void update(std::span<const Index> held, std::span<Tolerance> values, const Batch &batch) noexcept
{
    for (std::size_t next = 0; next < batch.size(); ++next)
    {
        batch.apply(next, values[position(held, batch.index(next))]);
    }
}

std::size_t merged_size(std::span<const Index> held, const Batch &batch) noexcept
{
    std::size_t size = held.size();
    for (std::size_t next = 0; next < batch.size(); ++next)
    {
        if (!batch.shadowed(next) && !std::ranges::binary_search(held, batch.index(next)))
        {
            ++size;
        }
    }
    return size;
}

void grow(std::vector<Index> &indices, std::vector<Tolerance> &values, std::size_t size)
{
    if (size > indices.capacity())
    {
        const std::size_t room = (std::max)(size, 2 * indices.capacity());
        indices.reserve(room);
        values.reserve(room);
    }
}

struct Cursor
{
    std::span<Index> indices;
    std::span<Tolerance> values;
    std::size_t held;
    std::size_t write;

    void shift(Index index) noexcept;
    Tolerance take(Index index) noexcept;
    void put(Index index, Tolerance value) noexcept;
};

void Cursor::shift(Index index) noexcept
{
    while (held > 0 && indices[held - 1] > index)
    {
        --held;
        put(indices[held], values[held]);
    }
}

Tolerance Cursor::take(Index index) noexcept
{
    if (held > 0 && indices[held - 1] == index)
    {
        --held;
        return values[held];
    }
    return kGlobal;
}

void Cursor::put(Index index, Tolerance value) noexcept
{
    --write;
    indices[write] = index;
    values[write] = value;
}

void merge(Cursor cursor, const Batch &batch) noexcept
{
    for (std::size_t left = batch.size(); left > 0; --left)
    {
        const std::size_t next = left - 1;
        if (!batch.shadowed(next))
        {
            const Index index = batch.index(next);
            cursor.shift(index);
            Tolerance value = cursor.take(index);
            batch.apply(next, value);
            cursor.put(index, value);
        }
    }
}

void insert(std::vector<Index> &indices, std::vector<Tolerance> &values, Batch &batch)
{
    if (!std::ranges::is_sorted(batch.indices))
    {
        batch.order = order_of(batch.indices);
    }
    const std::size_t held = indices.size();
    const std::size_t size = merged_size(indices, batch);
    grow(indices, values, size);
    indices.resize(size);
    values.resize(size);
    merge({.indices = indices, .values = values, .held = held, .write = size}, batch);
}

Index marked(Index index) noexcept
{
    return -index - 1;
}

Index unmarked(Index held) noexcept
{
    return held < 0 ? -held - 1 : held;
}

void mark(std::span<Index> held, Index index) noexcept
{
    const auto found = std::ranges::lower_bound(held, index, {}, unmarked);
    if (found != held.end() && *found == index)
    {
        *found = marked(index);
    }
}

template <typename Rule>
void compact(std::vector<Index> &indices, std::vector<Tolerance> &values, Rule rule) noexcept
{
    std::size_t kept = 0;
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        const Index to = rule(indices[k]);
        if (to >= 0)
        {
            indices[kept] = to;
            values[kept] = values[k];
            ++kept;
        }
    }
    indices.resize(kept);
    values.resize(kept);
}

}

Tolerance RecordTolerances::at(Index index) const noexcept
{
    const std::size_t found = position(indices_, index);
    const bool held = found < indices_.size() && indices_[found] == index;
    return held ? values_[found] : kGlobal;
}

void RecordTolerances::set(std::string_view noun,
                           Index size,
                           std::span<const Index> indices,
                           std::span<const Real> absolute,
                           std::span<const Real> relative)
{
    Batch batch{.indices = indices, .absolute = absolute, .relative = relative, .order = {}};
    check(noun, size, batch);
    if (indices.empty() || (absolute.empty() && relative.empty()))
    {
        return;
    }
    if (holds_all(indices_, batch))
    {
        update(indices_, values_, batch);
        return;
    }
    insert(indices_, values_, batch);
}

void RecordTolerances::remove(std::string_view noun, Index size, std::span<const Index> indices)
{
    for (const Index index : indices)
    {
        check_index(noun, index, size);
    }
    for (const Index index : indices)
    {
        mark(indices_, index);
    }
    compact(indices_, values_, std::identity{});
}

void RecordTolerances::renumber(std::span<const Index> map) noexcept
{
    compact(indices_, values_, [map](Index index) { return map[to_size(index)]; });
}

}
