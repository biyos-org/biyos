#pragma once

#include "core/numbers.hpp"
#include "io/lines.hpp"
#include "io/native/utils/record.hpp"

#include <string>

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

}
