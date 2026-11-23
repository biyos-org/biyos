#pragma once

#include "core/index.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace biyos
{

class Directory
{
  public:
    std::optional<Index> find(std::string_view name) const;
    bool add(std::string_view name);
    Index size() const noexcept;
    std::span<const std::string_view> names() const noexcept;

  private:
    struct Hash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view text) const noexcept;
    };

    std::unordered_map<std::string, Index, Hash, std::equal_to<>> indices_;
    std::vector<std::string_view> names_;
};

}
