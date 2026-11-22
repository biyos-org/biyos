#pragma once

#include "core/events.hpp"
#include "core/options.hpp"

#include <span>
#include <vector>

namespace biyos
{

class Settings
{
  public:
    Settings();
    explicit Settings(std::span<const OptionSpec> table);

    void set(Option option, OptionValue value, Events &events);
    const OptionValue *find(Option option) const noexcept;
    void erase(Option option) noexcept;

  private:
    struct Entry
    {
        Option option;
        OptionValue value;
    };

    std::span<const OptionSpec> table_;
    std::vector<Entry> entries_;
};

}
