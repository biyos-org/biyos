#pragma once

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/nl/source/source.hpp"
#include "io/nl/tokens.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos::nl
{

class TextSource final : public Source
{
  public:
    explicit TextSource(Lines &lines);

    bool next() override;
    void item(std::string_view part) override;
    void line(std::string_view part) override;
    char key() const noexcept override;
    std::string key_text() const override;
    void expect(std::size_t n) const override;
    Offset integer() override;
    Real real() override;
    std::string name() override;
    Offset code() override;
    bool is_constant(char key) const noexcept override;
    Real constant(char leaf) override;
    std::string last_field() const override;
    std::string where() const override;
    void inform(Stream stream, const std::string &text) const override;

  private:
    void start(bool keyed);
    std::size_t size() const noexcept;
    std::string_view take();

    Lines &lines_;
    Entry entry_;
    Fields fields_;
    bool keyed_ = false;
    std::size_t taken_ = 0;
    std::string_view last_;
};

}
