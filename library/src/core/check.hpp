#pragma once

#include "core/error.hpp"
#include "core/events/stream.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/text/name.hpp"

#include <concepts>
#include <string_view>
#include <type_traits>

namespace biyos
{

class Checker
{
  public:
    Checker(Stream stream, const Lookup &lookup, StaticName module);

    bool active(Checking cost) const noexcept
    {
        return cost != Checking::None && cost <= checking_;
    }

    template <typename Holds, typename Values>
        requires std::predicate<Holds &> && std::invocable<Values &> &&
                 std::convertible_to<std::invoke_result_t<Values &>, std::string_view>
    void require(Checking cost,
                 ErrorKind kind,
                 std::string_view invariant,
                 std::string_view object,
                 Holds &&holds,
                 Values &&values) const
    {
        if (active(cost) && !holds())
        {
            fail(kind, invariant, object, values());
        }
    }

  private:
    [[noreturn]] void fail(ErrorKind kind,
                           std::string_view invariant,
                           std::string_view object,
                           std::string_view values) const;

    Stream stream_;
    std::string_view module_;
    Checking checking_;
};

}
