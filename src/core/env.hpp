#pragma once

#include "core/error.hpp"
#include "core/events/deliveries.hpp"
#include "core/events/events.hpp"
#include "core/events/records.hpp"
#include "core/memory/usage.hpp"
#include "core/options/options.hpp"
#include "core/timing/clock.hpp"
#include "core/timing/interrupt.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

namespace biyos
{

class Env;

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
    Deliveries::State *delivery_;
    ErrorScope *outer_;
    Record kept_;
    bool swapped_ = false;
};

class Env
{
  public:
    Env();
    explicit Env(std::unique_ptr<Clock> clock);
    Env(std::unique_ptr<Clock> clock, std::span<const OptionSpec> table);
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

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

    RecordView last_error() const noexcept;
    void record_error(ErrorKind kind,
                      std::string_view text,
                      const Session *model = nullptr) noexcept;

    std::size_t handles() const noexcept;

    Usage &usage() noexcept;

  private:
    friend class ErrorScope;

    ErrorScope *scope() noexcept;

    std::unique_ptr<Clock> clock_;
    Events events_;
    Options options_;
    Interrupt interrupt_;
    Records records_;
    std::atomic<std::size_t> handles_{0};
    Usage usage_;
};

}
