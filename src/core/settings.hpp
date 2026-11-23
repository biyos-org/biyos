#pragma once

#include "core/events.hpp"
#include "core/options.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

class Settings
{
  public:
    Settings();
    explicit Settings(std::span<const OptionSpec> table);

    void set_bool(Option option, bool value, Events &events, const BIYOS_model *model = nullptr);
    void set_int(Option option, int value, Events &events, const BIYOS_model *model = nullptr);
    void set_double(Option option,
                    double value,
                    Events &events,
                    const BIYOS_model *model = nullptr);
    void set_string(Option option,
                    std::string_view value,
                    Events &events,
                    const BIYOS_model *model = nullptr);
    void set_flags(Option option, Masks value, Events &events, const BIYOS_model *model = nullptr);

    const OptionValue *find(Option option) const noexcept;
    void erase(Option option) noexcept;

  private:
    void store(Option option, OptionValue value, Events &events, const BIYOS_model *model);

    struct Entry
    {
        Option option;
        OptionValue value;
    };

    std::span<const OptionSpec> table_;
    std::vector<Entry> entries_;
};

}
