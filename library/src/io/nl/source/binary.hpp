#pragma once

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/nl/source/source.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>
#include <string_view>

namespace biyos::nl
{

class BinarySource final : public Source
{
  public:
    BinarySource(std::istream &input, std::string file, std::endian order, Offset bytes);

    bool next() override;
    void item(std::string_view part) override;
    void line(std::string_view part) override;
    char key() const noexcept override;
    std::string head() const override;
    void expect(std::size_t n) const override;
    Offset integer() override;
    Real real() override;
    std::string name() override;
    Offset code() override;
    bool holds(char leaf) const noexcept override;
    Real constant(char leaf) override;
    std::string token() const override;
    std::string where() const override;
    void inform(Stream stream, const std::string &text) const override;

  private:
    std::size_t take(char *data, std::size_t n);
    std::uint64_t bits(std::size_t n);
    char byte();
    [[noreturn]] void cut_short() const;
    void begin(std::string_view part);
    Offset small();

    std::istream &input_;
    std::string file_;
    std::endian order_;
    Offset bytes_;
    Offset at_;
    Offset start_;
    std::string part_;
    char key_ = '\0';
    std::string token_;
};

}
