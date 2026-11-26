#pragma once

#include "core/numbers.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"

#include <string_view>
#include <type_traits>

namespace biyos
{

struct Found
{
    const OptionValue &value;
    OptionSource source;
};

class Lookup
{
  public:
    Lookup(const Settings &settings, const Options &options) noexcept;
    Lookup(Settings &&settings, const Options &options) = delete;
    explicit Lookup(const Options &options) noexcept;

    const Options &options() const noexcept;
    Found find(Option option) const;

    bool get_bool(Option option) const;
    int get_int(Option option) const;
    Real get_double(Option option) const;
    std::string_view get_string(Option option) const;
    Masks get_flags(Option option) const;
    PartState part_state(Option option, Mask part) const;

    template <typename E>
        requires std::is_enum_v<E>
    E get_enum(Option option) const
    {
        return static_cast<E>(enum_value(option));
    }

  private:
    int enum_value(Option option) const;

    const Settings *settings_;
    const Options &options_;
};

}
