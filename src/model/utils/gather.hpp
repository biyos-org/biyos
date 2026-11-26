#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace biyos
{

template <typename Record>
std::vector<Record> picked(const Model &model,
                           const Record &(Model::*read)(Index) const,
                           std::span<const Index> indices)
{
    std::vector<Record> found;
    found.reserve(indices.size());
    for (const Index i : indices)
    {
        found.push_back((model.*read)(i));
    }
    return found;
}

template <typename T> struct Gathered
{
    std::vector<Offset> starts;
    std::vector<T> items;
};

template <typename T> using PartReader = std::span<const T> (Model::*)(Index, Index) const;

template <typename Read> auto gathered(std::span<const Index> records, Read read)
{
    using Item = typename std::invoke_result_t<Read, Index>::value_type;
    Gathered<Item> found{.starts = {0}, .items = {}};
    for (const Index record : records)
    {
        const auto part = read(record);
        found.items.insert(found.items.end(), part.begin(), part.end());
        found.starts.push_back(to_offset(found.items.size()));
    }
    return found;
}

template <typename T>
Gathered<T> gathered(const Model &model, PartReader<T> part, std::span<const Index> records)
{
    return gathered(records, [&](Index record) { return (model.*part)(record, record + 1); });
}

template <typename T>
std::size_t item_count(const Model &model, PartReader<T> part, std::span<const Index> records)
{
    std::size_t count = 0;
    for (const Index record : records)
    {
        count += (model.*part)(record, record + 1).size();
    }
    return count;
}

}
