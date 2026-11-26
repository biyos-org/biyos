#pragma once

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/lines.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace biyos::native
{

class Record
{
  public:
    explicit Record(const Lines &lines) noexcept;

    std::string_view word();
    Index index();
    int integer();
    Offset count();
    Offset entries(std::size_t width);
    Real real();
    std::optional<Real> tolerance();
    std::string name();
    void end() const;
    [[noreturn]] void refuse(const std::string &message) const;

  private:
    std::string_view take();

    const Lines &lines_;
    std::size_t next_ = 0;
};

template <typename Kind, std::size_t N>
Kind kind_of(Record &record, const std::array<Kind, N> &kinds, std::string_view noun)
{
    const int code = record.integer();
    for (const Kind kind : kinds)
    {
        if (static_cast<int>(kind) == code)
        {
            return kind;
        }
    }
    record.refuse(std::string(noun) + " " + text_of(code) + " is unknown");
}

}
