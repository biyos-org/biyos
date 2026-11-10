#pragma once

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

    const std::string &last_error() const noexcept;
    void record_error(std::string_view text) noexcept;

  private:
    Options options_;
    std::string last_error_;
};

}
