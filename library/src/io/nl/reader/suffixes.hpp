#pragma once

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/nl/header.hpp"
#include "io/nl/source/source.hpp"

namespace biyos::nl
{

class Suffixes
{
  public:
    Suffixes(Stream stream, Source &source, const Header &header);

    void read();

  private:
    Index size_of(Offset kind) const noexcept;

    Stream stream_;
    Source &source_;
    const Header &header_;
};

}
