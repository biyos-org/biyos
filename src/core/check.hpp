#pragma once

#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/options.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

namespace biyos
{

// A check runs when its level is cheap or full and at most check_level, read once when the
// checker is built: below its level it costs one compare when the level is a constant at the
// call site. holds is called only when the check runs, values only when holds is false. object is
// a fixed name, taken even by a check that does not run, so detail such as an index goes in
// values. A failure records its error on the environment, emits one check event from the
// checker's module unless an event is being delivered, then throws. Inside a fill it only
// throws, so the error is dropped with the event being filled.
class Checker
{
  public:
    Checker(Environment &env, std::string_view module);

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

    Environment &env_;
    std::string module_;
    CheckLevel level_;
};

}
