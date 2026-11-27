#pragma once

#include "core/numbers.hpp"
#include "io/lines.hpp"
#include "io/native/utils/words.hpp"
#include "io/records.hpp"

#include <algorithm>
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

template <typename Value, std::size_t N>
Value lookup(Record &record, const std::array<Word<Value>, N> &words, std::string_view noun)
{
    const std::string_view text = record.word();
    const auto found = std::ranges::find(words, text, &Word<Value>::text);
    if (found == words.end())
    {
        record.refuse(quote(text) + " is not " + std::string(noun));
    }
    return found->value;
}

}
