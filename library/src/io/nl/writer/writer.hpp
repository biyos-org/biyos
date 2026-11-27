#pragma once

#include "core/events/stream.hpp"
#include "io/nl/writer/defined.hpp"
#include "io/nl/writer/entries.hpp"
#include "io/nl/writer/layout.hpp"
#include "io/nl/writer/printer.hpp"
#include "io/nl/writer/usage.hpp"
#include "model/model.hpp"

#include <ostream>
#include <string>

namespace biyos::nl
{

class Plan
{
  public:
    explicit Plan(const Model &model);

    void print(std::ostream &output) const;
    void report(Stream stream, const std::string &file) const;

  private:
    void print_header(std::ostream &output) const;
    void print_nonlinear(std::ostream &output) const;
    void print_records(std::ostream &output, Printer &printer) const;
    void print_ranges(std::ostream &output) const;
    void print_pairing(std::ostream &output, const Complement &complement) const;
    void print_bounds(std::ostream &output) const;
    void print_columns(std::ostream &output) const;
    void print_entries(std::ostream &output) const;
    Index function_of(Index row) const noexcept;
    Index n_rows() const noexcept;

    const Model &model_;
    Records records_;
    Usage usage_;
    Columns columns_;
    Entries entries_;
    Defined defined_;
};

}
