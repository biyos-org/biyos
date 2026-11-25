#pragma once

#include "core/events/events.hpp"
#include "core/events/hooks.hpp"
#include "core/numbers.hpp"
#include "core/options/options.hpp"
#include "core/text/strings.hpp"

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
    Settings(const Settings &other);
    Settings &operator=(const Settings &) = delete;

    void set_bool(Option option,
                  bool value,
                  Events &events,
                  const BIYOS_model *model = nullptr,
                  Hooks *hooks = nullptr);
    void set_int(Option option,
                 int value,
                 Events &events,
                 const BIYOS_model *model = nullptr,
                 Hooks *hooks = nullptr);
    void set_double(Option option,
                    Real value,
                    Events &events,
                    const BIYOS_model *model = nullptr,
                    Hooks *hooks = nullptr);
    void set_string(Option option,
                    std::string_view value,
                    Events &events,
                    const BIYOS_model *model = nullptr,
                    Hooks *hooks = nullptr);
    void set_flags(Option option,
                   Masks value,
                   Events &events,
                   const BIYOS_model *model = nullptr,
                   Hooks *hooks = nullptr);

    const OptionValue *find(Option option) const;
    void erase(Option option);
    bool empty() const noexcept;

  private:
    void store(Option option,
               const OptionValue &value,
               Events &events,
               const BIYOS_model *model,
               Hooks *hooks);

    struct Entry
    {
        Option option;
        OptionValue value;
    };

    std::span<const OptionSpec> table_;
    Strings strings_;
    std::vector<Entry> entries_;
};

}
