#pragma once

#include "core/clock.hpp"
#include "core/events.hpp"
#include "core/options.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace biyos
{

class Environment
{
  public:
    Environment();
    explicit Environment(std::unique_ptr<Clock> clock);
    Environment(const Environment &) = delete;
    Environment &operator=(const Environment &) = delete;

    // A failure recorded inside a scope is undone when the scope ends, so a subscriber's failed
    // call never changes the last error another reads. The outer message is swapped aside on the
    // first failure only, so a scope without one allocates nothing.
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

    Options &options() noexcept;
    const Options &options() const noexcept;

    const Clock &clock() const noexcept;

    Events &events() noexcept;
    const Events &events() const noexcept;

    const std::string &last_error() const noexcept;
    void record_error(std::string_view text) noexcept;

  private:
    Options options_;
    std::unique_ptr<Clock> clock_;
    Events events_;
    std::string last_error_;
    ErrorScope *scope_ = nullptr;
};

}
