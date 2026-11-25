#include "model/attributes/names.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "core/text/utf8.hpp"
#include "model/checks.hpp"
#include "model/renumber.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

std::string at_byte(std::string_view fault, std::size_t at)
{
    return "name " + std::string(fault) + " at byte " + text_of(at);
}

void check_text(std::string_view noun, Index index, std::string_view name)
{
    const std::string fault = name_fault(name);
    if (!fault.empty())
    {
        refuse(noun, index, fault);
    }
}

void check_entries(std::string_view noun,
                   Index size,
                   std::span<const Index> indices,
                   std::span<const std::string_view> names)
{
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(noun, indices[k], size);
        check_text(noun, indices[k], names[k]);
    }
}

std::vector<std::size_t> last_entries(std::span<const Index> indices)
{
    std::vector<std::size_t> order(indices.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    const auto index_at = [indices](std::size_t k) { return indices[k]; };
    std::ranges::stable_sort(order, std::ranges::less{}, index_at);
    std::vector<std::size_t> entries;
    for (std::size_t j = 0; j < order.size(); ++j)
    {
        if (j + 1 == order.size() || index_at(order[j + 1]) != index_at(order[j]))
        {
            entries.push_back(order[j]);
        }
    }
    return entries;
}

}

std::string name_fault(std::string_view name)
{
    if (name.empty())
    {
        return "name is empty";
    }
    std::size_t at = 0;
    while (at < name.size())
    {
        const utf8::Sequence sequence = utf8::first_sequence(name.substr(at));
        if (name[at] == '\0')
        {
            return at_byte("holds a NUL", at);
        }
        if (!sequence.valid)
        {
            return at_byte("is not valid UTF-8", at);
        }
        at += sequence.size;
    }
    return "";
}

std::size_t Names::Hash::operator()(std::string_view text) const noexcept
{
    return std::hash<std::string_view>{}(text);
}

Names::Names(const Names &other) : holders_(other.holders_), names_(other.names_.size(), nullptr)
{
    for (std::size_t k = 0; k < other.names_.size(); ++k)
    {
        if (other.names_[k] != nullptr)
        {
            names_[k] = &holders_.find(*other.names_[k])->first;
        }
    }
}

Names &Names::operator=(const Names &other)
{
    *this = Names(other);
    return *this;
}

const std::string *Names::name(Index index) const noexcept
{
    return to_size(index) < names_.size() ? names_[to_size(index)] : nullptr;
}

Holders Names::find(std::string_view name) const
{
    const auto found = holders_.find(name);
    if (found == holders_.end())
    {
        return Holders{.count = 0, .index = -1};
    }
    const Tally &tally = found->second;
    const Index lone = tally.count == 1 ? static_cast<Index>(tally.index_sum) : -1;
    return Holders{.count = tally.count, .index = lone};
}

void Names::set(std::string_view noun,
                Index size,
                std::span<const Index> indices,
                std::span<const std::string_view> names)
{
    if (indices.size() != names.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and names differ in number");
    }
    if (indices.empty())
    {
        return;
    }
    check_entries(noun, size, indices, names);
    const std::vector<std::size_t> entries = last_entries(indices);
    Table fresh = missing(names, entries);
    std::vector<const std::string *> given(entries.size(), nullptr);
    grow(indices[entries.back()]);
    holders_.reserve(holders_.size() + fresh.size());
    for (std::size_t j = 0; j < entries.size(); ++j)
    {
        const Index target = indices[entries[j]];
        given[j] = names_[to_size(target)];
        acquire(target, names[entries[j]], fresh);
    }
    for (std::size_t j = 0; j < entries.size(); ++j)
    {
        release(indices[entries[j]], given[j]);
    }
}

void Names::remove(std::string_view noun, Index size, std::span<const Index> indices)
{
    for (const Index i : indices)
    {
        check_index(noun, i, size);
    }
    for (const Index i : indices)
    {
        if (to_size(i) < names_.size())
        {
            release(i, names_[to_size(i)]);
            names_[to_size(i)] = nullptr;
        }
    }
}

void Names::renumber(std::span<const Index> map) noexcept
{
    std::size_t kept = 0;
    for (std::size_t k = 0; k < names_.size(); ++k)
    {
        const auto index = to_index(k);
        if (map[k] == kDeleted)
        {
            release(index, names_[k]);
        }
        else
        {
            shift(index, map[k], names_[k]);
            names_[kept] = names_[k];
            ++kept;
        }
    }
    names_.resize(kept);
}

Names::Table Names::missing(std::span<const std::string_view> names,
                            std::span<const std::size_t> entries) const
{
    Table fresh;
    for (const std::size_t k : entries)
    {
        if (!holders_.contains(names[k]))
        {
            fresh.try_emplace(std::string(names[k]), Tally{.count = 0, .index_sum = 0});
        }
    }
    return fresh;
}

void Names::grow(Index index)
{
    names_.resize((std::max)(names_.size(), to_size(index) + 1));
}

void Names::acquire(Index index, std::string_view name, Table &fresh)
{
    auto found = holders_.find(name);
    if (found == holders_.end())
    {
        found = holders_.insert(fresh.extract(fresh.find(name))).position;
    }
    found->second.count += 1;
    found->second.index_sum += index;
    names_[to_size(index)] = &found->first;
}

void Names::shift(Index from, Index to, const std::string *name) noexcept
{
    if (name != nullptr)
    {
        holders_.find(*name)->second.index_sum -= from - to;
    }
}

void Names::release(Index index, const std::string *name) noexcept
{
    if (name == nullptr)
    {
        return;
    }
    const auto found = holders_.find(*name);
    found->second.count -= 1;
    found->second.index_sum -= index;
    if (found->second.count == 0)
    {
        holders_.erase(found);
    }
}

}
