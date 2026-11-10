#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace biyos
{

enum class Option
{
    Seed = 1,
    TimeLimit,
    CheckLevel,
    Presolve,
    Reproducer,
};

Option find_option(std::string_view name);
Option find_option(int id);
std::string_view name_of(Option option) noexcept;

class Options
{
  public:
    Options();

    void set_bool(Option option, bool value);
    void set_int(Option option, int value);
    void set_double(Option option, double value);
    void set_string(Option option, std::string_view value);

    bool get_bool(Option option) const;
    int get_int(Option option) const;
    double get_double(Option option) const;
    const std::string &get_string(Option option) const;

  private:
    using Value = std::variant<bool, int, double, std::string>;

    std::vector<Value> values_;
};

}
