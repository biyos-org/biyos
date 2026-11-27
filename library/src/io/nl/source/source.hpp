#pragma once

#include "core/env/error.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/utils/lines.hpp"

#include <cstddef>
#include <istream>
#include <string>
#include <string_view>

namespace biyos::nl
{

class Source
{
  public:
    virtual ~Source() = default;

    virtual bool next() = 0;
    virtual void item(std::string_view part) = 0;
    virtual void line(std::string_view part) = 0;
    virtual char key() const noexcept = 0;
    virtual std::string head() const = 0;
    virtual void expect(std::size_t n) const = 0;
    virtual Offset integer() = 0;
    virtual Real real() = 0;
    virtual std::string name() = 0;
    virtual Offset code() = 0;
    virtual bool holds(char leaf) const noexcept = 0;
    virtual Real constant(char leaf) = 0;
    virtual std::string token() const = 0;
    virtual std::string where() const = 0;
    virtual void inform(Stream stream, const std::string &text) const = 0;

    Index count();
    Index index(Index size, std::string_view noun);
    [[noreturn]] void bad(const std::string &message) const;
    [[noreturn]] void refuse(const std::string &message) const;
};

[[noreturn]] void reject(const Source &source, const Error &error);
Offset size_of(std::istream &input);

}
