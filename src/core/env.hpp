#pragma once

#include "core/clock.hpp"
#include "core/events.hpp"
#include "core/interrupt.hpp"
#include "core/options.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

class Env
{
  public:
    Env();
    explicit Env(std::unique_ptr<Clock> clock);
    Env(std::unique_ptr<Clock> clock, std::span<const OptionSpec> table);
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    class ErrorScope
    {
      public:
        explicit ErrorScope(Env &env) noexcept;
        ~ErrorScope();
        ErrorScope(const ErrorScope &) = delete;
        ErrorScope &operator=(const ErrorScope &) = delete;

      private:
        friend class Env;

        Env &env_;
        ErrorScope *outer_;
        std::string kept_;
        bool swapped_ = false;
    };

    class Handle
    {
      public:
        explicit Handle(Env &env) noexcept;
        ~Handle();
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;

        Env &env() const noexcept;

      private:
        Env &env_;
    };

    Options &options() noexcept;
    const Options &options() const noexcept;

    const Clock &clock() const noexcept;

    Events &events() noexcept;
    const Events &events() const noexcept;

    Interrupt &interrupt() noexcept;
    const Interrupt &interrupt() const noexcept;

    const std::string &last_error() const noexcept;
    void record_error(std::string_view text) noexcept;

    std::size_t handles() const noexcept;

  private:
    std::unique_ptr<Clock> clock_;
    Events events_;
    Options options_;
    Interrupt interrupt_;
    std::string last_error_;
    ErrorScope *scope_ = nullptr;
    std::size_t handles_ = 0;
};

}
