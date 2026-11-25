#pragma once

#include "core/events/stream.hpp"
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

    void set_bool(Option option, bool value, Stream stream);
    void set_int(Option option, int value, Stream stream);
    void set_double(Option option, Real value, Stream stream);
    void set_string(Option option, std::string_view value, Stream stream);
    void set_flags(Option option, Masks value, Stream stream);

    const OptionValue *find(Option option) const;
    void erase(Option option);
    bool empty() const noexcept;

  private:
    void store(Option option, const OptionValue &value, Stream stream);

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
