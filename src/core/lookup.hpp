#pragma once

#include "core/options.hpp"
#include "core/settings.hpp"

#include <cstddef>
#include <string>
#include <type_traits>

namespace biyos
{

struct Found
{
    const OptionValue &value;
    Source source;
};

class Lookup
{
  public:
    Lookup(const Settings &settings, const Options &options) noexcept;
    Lookup(Settings &&settings, const Options &options) = delete;
    explicit Lookup(const Options &options) noexcept;

    Found find(Option option) const;

    bool get_bool(Option option) const;
    int get_int(Option option) const;
    double get_double(Option option) const;
    const std::string &get_string(Option option) const;

    template <typename E>
        requires std::is_enum_v<E>
    E get_enum(Option option) const
    {
        return static_cast<E>(position(option));
    }

  private:
    std::size_t position(Option option) const;

    const Settings &settings_;
    const Options &options_;
};

}
