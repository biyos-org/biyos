#pragma once

#include "core/numbers.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace biyos
{

std::string name_fault(std::string_view name);

struct Holders
{
    std::int64_t count;
    Index index;
};

class Names
{
  public:
    Names() = default;
    Names(const Names &other);
    Names(Names &&other) = default;
    Names &operator=(const Names &other);
    Names &operator=(Names &&other) = default;
    ~Names() = default;

    const std::string *name(Index index) const noexcept;
    Holders find(std::string_view name) const;
    void set(std::string_view noun,
             Index size,
             std::span<const Index> indices,
             std::span<const std::string_view> names);
    void remove(std::string_view noun, Index size, std::span<const Index> indices);
    void renumber(std::span<const Index> map) noexcept;

  private:
    struct Tally
    {
        std::int64_t count;
        std::int64_t index_sum;
    };
    struct Hash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view text) const noexcept;
    };
    using Table = std::unordered_map<std::string, Tally, Hash, std::equal_to<>>;

    Table missing(std::span<const std::string_view> names,
                  std::span<const std::size_t> entries) const;
    void grow(Index index);
    void acquire(Index index, std::string_view name, Table &fresh);
    void shift(Index from, Index to, const std::string *name) noexcept;
    void release(Index index, const std::string *name) noexcept;

    Table holders_;
    std::vector<const std::string *> names_;
};

}
