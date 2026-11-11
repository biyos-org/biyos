#pragma once

#include "core/clock.hpp"
#include "core/events.hpp"
#include "core/options.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

class Environment
{
  public:
    Environment();
    explicit Environment(std::unique_ptr<Clock> clock);
    Environment(std::unique_ptr<Clock> clock, std::span<const OptionSpec> table);
    Environment(const Environment &) = delete;
    Environment &operator=(const Environment &) = delete;

    class ErrorScope
    {
      public:
        explicit ErrorScope(Environment &env) noexcept;
        ~ErrorScope();
        ErrorScope(const ErrorScope &) = delete;
        ErrorScope &operator=(const ErrorScope &) = delete;

      private:
        friend class Environment;

        Environment &env_;
        ErrorScope *outer_;
        std::string kept_;
        bool swapped_ = false;
    };

    class Handle
    {
      public:
        explicit Handle(Environment &env) noexcept;
        ~Handle();
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;

        Environment &env() const noexcept;

      private:
        Environment &env_;
    };

    Options &options() noexcept;
    const Options &options() const noexcept;

    const Clock &clock() const noexcept;

    Events &events() noexcept;
    const Events &events() const noexcept;

    const std::string &last_error() const noexcept;
    void record_error(std::string_view text) noexcept;

    std::size_t handles() const noexcept;

  private:
    std::unique_ptr<Clock> clock_;
    Events events_;
    Options options_;
    std::string last_error_;
    ErrorScope *scope_ = nullptr;
    std::size_t handles_ = 0;
};

}
