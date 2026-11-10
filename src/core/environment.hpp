#pragma once

#include "core/events.hpp"
#include "core/options.hpp"

#include <string>
#include <string_view>

namespace biyos
{

class Environment
{
  public:
    Options &options() noexcept;
    const Options &options() const noexcept;

    Events &events() noexcept;
    const Events &events() const noexcept;

    const std::string &last_error() const noexcept;
    void record_error(std::string_view text) noexcept;

  private:
    Options options_;
    Events events_;
    std::string last_error_;
};

}
