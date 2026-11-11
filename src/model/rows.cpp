#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "model/checks.hpp"
#include "model/entries.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr auto kRoom = static_cast<std::size_t>((std::numeric_limits<Index>::max)());
constexpr std::string_view kNoun = "row";

[[noreturn]] void refuse_starts(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "argument 'starts'", reason);
}

void check_shape(std::size_t rows, std::span<const Offset> starts, std::size_t entries)
{
    if (starts.size() != rows + 1)
    {
        throw Error(ErrorKind::Internal, "model", "rows and starts differ in number");
    }
    check_starts(starts);
    if (starts.back() != static_cast<Offset>(entries))
    {
        throw Error(ErrorKind::Internal, "model", "starts and entries differ in number");
    }
}

void check_bounds(const std::string &object, const Row &row)
{
    const std::string_view fault = bound_fault(row.lower, row.upper);
    if (!fault.empty())
    {
        throw Error(ErrorKind::InvalidArgument, object, std::string(fault));
    }
}

std::span<Entry> slice(std::span<Entry> entries, std::span<const Offset> starts, std::size_t k)
{
    const auto begin = static_cast<std::size_t>(starts[k]);
    const auto end = static_cast<std::size_t>(starts[k + 1]);
    return entries.subspan(begin, end - begin);
}

template <typename T> void make_room(std::vector<T> &items, std::size_t extra)
{
    const std::size_t wanted = items.size() + extra;
    if (wanted > items.capacity())
    {
        const std::size_t doubled = (std::max)(wanted, 2 * items.capacity());
        items.reserve((std::min)(doubled, items.max_size()));
    }
}

}

void check_starts(std::span<const Offset> starts)
{
    if (!starts.empty() && starts.front() != 0)
    {
        refuse_starts("begins at " + text_of(starts.front()) + ", not 0");
    }
    for (std::size_t k = 1; k < starts.size(); ++k)
    {
        if (starts[k] < starts[k - 1])
        {
            const std::string fall = text_of(starts[k - 1]) + " to " + text_of(starts[k]);
            refuse_starts("decreases from " + fall + " at entry " + text_of(k));
        }
    }
}

Index Model::row_count() const noexcept
{
    return static_cast<Index>(rows_.size());
}

const Row &Model::row(Index index) const
{
    check_index(kNoun, index, row_count());
    return rows_[static_cast<std::size_t>(index)];
}

std::span<const Row> Model::rows(Index first, Index count) const
{
    check_span(kNoun, first, count, row_count());
    const auto start = static_cast<std::size_t>(first);
    return std::span(rows_).subspan(start, static_cast<std::size_t>(count));
}

std::span<const Entry> Model::entries(Index first, Index count) const
{
    check_span(kNoun, first, count, row_count());
    const auto begin = static_cast<std::size_t>(begin_of(static_cast<std::size_t>(first)));
    const auto end = static_cast<std::size_t>(begin_of(static_cast<std::size_t>(first + count)));
    return std::span(entries_).subspan(begin, end - begin);
}

void Model::check_room(std::size_t rows, Offset entries) const
{
    if (rows > kRoom - rows_.size())
    {
        throw Error(ErrorKind::NoMemory, "model", "no row index left");
    }
    if (entries > static_cast<Offset>(entries_.max_size() - entries_.size()))
    {
        throw Error(ErrorKind::NoMemory, "model", "no entry offset left");
    }
}

void Model::add_rows(std::span<const Row> added, std::span<const Offset> starts,
                     std::vector<Entry> entries)
{
    check_room(added.size(), static_cast<Offset>(entries.size()));
    check_shape(added.size(), starts, entries.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        const std::string object = item(kNoun, static_cast<Index>(rows_.size() + k));
        const std::span<Entry> row = slice(entries, starts, k);
        check_bounds(object, added[k]);
        check_entries(object, row, variable_count());
        sort_entries(object, row);
    }
    append(added, starts, entries);
}

void Model::set_rows(std::span<const Index> indices, std::span<const Row> changed)
{
    if (indices.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and rows differ in number");
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(kNoun, indices[k], row_count());
        check_bounds(item(kNoun, indices[k]), changed[k]);
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        rows_[static_cast<std::size_t>(indices[k])] = changed[k];
    }
}

Offset Model::begin_of(std::size_t row) const noexcept
{
    return row == 0 ? 0 : ends_[row - 1];
}

void Model::append(std::span<const Row> added, std::span<const Offset> starts,
                   std::span<const Entry> entries)
{
    make_room(rows_, added.size());
    make_room(ends_, added.size());
    make_room(entries_, entries.size());
    const auto base = static_cast<Offset>(entries_.size());
    rows_.insert(rows_.end(), added.begin(), added.end());
    for (const Offset end : starts.subspan(1))
    {
        ends_.push_back(base + end);
    }
    entries_.insert(entries_.end(), entries.begin(), entries.end());
}

}
