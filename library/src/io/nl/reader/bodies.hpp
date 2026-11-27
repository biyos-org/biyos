#pragma once

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "io/nl/reader/expressions.hpp"
#include "io/nl/source/source.hpp"
#include "model/model.hpp"

#include <optional>
#include <vector>

namespace biyos::nl
{

class Bodies
{
  public:
    explicit Bodies(Index functions);

    bool has_body(Index function) const noexcept;
    void set_body(Index function, const Operand &body);
    bool has_terms(Index function) const noexcept;
    void set_terms(Index function, std::vector<LinearTerm> terms);
    void add_to(const Source &source, Model &model) const;

  private:
    std::vector<std::optional<Operand>> bodies_;
    std::vector<std::optional<std::vector<LinearTerm>>> terms_;
};

}
