#include "model/names.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "core/utf8.hpp"
#include "model/checks.hpp"

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

std::size_t slot(Index idx) noexcept
{
    return static_cast<std::size_t>(idx);
}

std::string at_byte(std::string_view fault, std::size_t at)
{
    return "name " + std::string(fault) + " at byte " + text_of(at);
}

void check_text(std::string_view noun, Index idx, std::string_view name)
{
    const std::string fault = name_fault(name);
    if (!fault.empty())
    {
        refuse(noun, idx, fault);
    }
}

void check_entries(std::string_view noun,
                   Index size,
                   std::span<const Index> idx,
                   std::span<const std::string_view> names)
{
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        check_idx(noun, idx[k], size);
        check_text(noun, idx[k], names[k]);
    }
}

std::vector<std::size_t> last_entries(std::span<const Index> idx)
{
    std::vector<std::size_t> order(idx.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    const auto idx_at = [idx](std::size_t k) { return idx[k]; };
    std::ranges::stable_sort(order, std::ranges::less{}, idx_at);
    std::vector<std::size_t> entries;
    for (std::size_t j = 0; j < order.size(); ++j)
    {
        if (j + 1 == order.size() || idx_at(order[j + 1]) != idx_at(order[j]))
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

const std::string *Names::name(Index idx) const noexcept
{
    return slot(idx) < names_.size() ? names_[slot(idx)] : nullptr;
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
                std::span<const Index> idx,
                std::span<const std::string_view> names)
{
    if (idx.size() != names.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and names differ in number");
    }
    if (idx.empty())
    {
        return;
    }
    check_entries(noun, size, idx, names);
    const std::vector<std::size_t> entries = last_entries(idx);
    Table fresh = missing(names, entries);
    std::vector<const std::string *> given(entries.size(), nullptr);
    grow(idx[entries.back()]);
    holders_.reserve(holders_.size() + fresh.size());
    for (std::size_t j = 0; j < entries.size(); ++j)
    {
        const Index target = idx[entries[j]];
        given[j] = names_[slot(target)];
        acquire(target, names[entries[j]], fresh);
    }
    for (std::size_t j = 0; j < entries.size(); ++j)
    {
        release(idx[entries[j]], given[j]);
    }
}

void Names::remove(std::string_view noun, Index size, std::span<const Index> idx)
{
    for (const Index i : idx)
    {
        check_idx(noun, i, size);
    }
    for (const Index i : idx)
    {
        if (slot(i) < names_.size())
        {
            release(i, names_[slot(i)]);
            names_[slot(i)] = nullptr;
        }
    }
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

void Names::grow(Index idx)
{
    names_.resize((std::max)(names_.size(), slot(idx) + 1));
}

void Names::acquire(Index idx, std::string_view name, Table &fresh)
{
    auto found = holders_.find(name);
    if (found == holders_.end())
    {
        found = holders_.insert(fresh.extract(fresh.find(name))).position;
    }
    found->second.count += 1;
    found->second.index_sum += idx;
    names_[slot(idx)] = &found->first;
}

void Names::release(Index idx, const std::string *name) noexcept
{
    if (name == nullptr)
    {
        return;
    }
    const auto found = holders_.find(*name);
    found->second.count -= 1;
    found->second.index_sum -= idx;
    if (found->second.count == 0)
    {
        holders_.erase(found);
    }
}

}
