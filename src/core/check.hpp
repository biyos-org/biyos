#pragma once

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/lookup.hpp"
#include "core/name.hpp"
#include "core/options.hpp"

#include <concepts>
#include <string_view>
#include <type_traits>

namespace biyos
{

class Checker
{
  public:
    Checker(Env &env, const Lookup &lookup, StaticName module);

    bool active(CheckLevel level) const noexcept
    {
        return level != CheckLevel::None && level <= level_;
    }

    template <typename Holds, typename Values>
        requires std::predicate<Holds &> && std::invocable<Values &> &&
                 std::convertible_to<std::invoke_result_t<Values &>, std::string_view>
    void require(CheckLevel level,
                 ErrorKind kind,
                 std::string_view invariant,
                 std::string_view object,
                 Holds &&holds,
                 Values &&values) const
    {
        if (active(level) && !holds())
        {
            fail(kind, invariant, object, values());
        }
    }

  private:
    [[noreturn]] void fail(ErrorKind kind,
                           std::string_view invariant,
                           std::string_view object,
                           std::string_view values) const;

    Env &env_;
    std::string_view module_;
    CheckLevel level_;
};

}
