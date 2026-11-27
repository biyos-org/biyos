#pragma once

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/nl/header.hpp"
#include "io/nl/source/source.hpp"
#include "model/model.hpp"

#include <optional>
#include <vector>

namespace biyos::nl
{

struct Weights
{
    Place place;
    std::vector<std::optional<Real>> values;
};

class Suffixes
{
  public:
    Suffixes(Stream stream, Source &source, const Header &header);

    void read();
    void add_to(Model &model) const;

  private:
    void read_numbers(Offset kind, Index n);
    void read_weights(Offset kind, Index n);
    void skip(Offset kind, Index n);
    Index read_variable(std::vector<bool> &seen);
    Index size_of(Offset kind) const noexcept;

    Stream stream_;
    Source &source_;
    const Header &header_;
    std::optional<std::vector<Offset>> numbers_;
    std::optional<Weights> weights_;
};

}
