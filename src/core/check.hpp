#pragma once

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/options.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

namespace biyos
{

class Checker
{
  public:
    Checker(Env &env, std::string_view module);

    bool active(CheckLevel lvl) const noexcept { return lvl != CheckLevel::None && lvl <= level_; }

    template <typename Holds, typename Values>
        requires std::predicate<Holds &> && std::invocable<Values &> &&
                 std::convertible_to<std::invoke_result_t<Values &>, std::string_view>
    void require(CheckLevel lvl,
                 ErrorKind knd,
                 std::string_view invariant,
                 std::string_view object,
                 Holds &&holds,
                 Values &&values) const
    {
        if (active(lvl) && !holds())
        {
            fail(knd, invariant, object, values());
        }
    }

  private:
    [[noreturn]] void fail(ErrorKind knd,
                           std::string_view invariant,
                           std::string_view object,
                           std::string_view values) const;

    Env &env_;
    std::string module_;
    CheckLevel level_;
};

}
