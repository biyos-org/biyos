#pragma once

#include "core/numbers.hpp"
#include "io/native/utils/record.hpp"
#include "io/utils/lines.hpp"

#include <string>
#include <string_view>

namespace biyos::native
{

class Body
{
  public:
    Body(Lines &lines, std::string word, Offset count);

    bool next();
    void follow();
    Record record() const noexcept;

  private:
    Lines &lines_;
    std::string word_;
    Offset left_;
};

bool next_line(Lines &lines);
void read_start(Lines &lines, std::string_view whole, std::string_view magic, int newest);

}
