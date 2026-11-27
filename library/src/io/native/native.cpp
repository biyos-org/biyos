#include "io/native/native.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "io/native/parts.hpp"
#include "io/native/readers/attributes.hpp"
#include "io/native/readers/functions.hpp"
#include "io/native/readers/records.hpp"
#include "io/native/utils/body.hpp"
#include "io/native/utils/record.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace biyos
{

namespace
{

using native::Body;
using native::kMagic;
using native::kVersion;
using native::next_line;
using native::Part;
using native::read_start;
using native::Record;

struct Key
{
    Part part;
    std::size_t family;

    auto operator<=>(const Key &) const = default;
};

Family family_of(Record &record)
{
    const std::string_view noun = record.word();
    const auto found = std::ranges::find(kFamilies, noun, noun_of);
    if (found == kFamilies.end())
    {
        record.refuse(quote(noun) + " is not a family");
    }
    return *found;
}

class NativeReader
{
  public:
    NativeReader(Lines &lines, std::string_view whole);

    Model read();

  private:
    void section();
    void enter(Key key, const std::string &word);
    void apply(Record &header, Part part, const std::string &word);
    void fill(Body &body, Part part, Family family);
    void rest();

    Lines &lines_;
    std::string_view whole_;
    Model model_;
    std::optional<Key> last_;
};

NativeReader::NativeReader(Lines &lines, std::string_view whole) : lines_(lines), whole_(whole) {}

Model NativeReader::read()
{
    read_start(lines_, whole_, kMagic, kVersion);
    while (last_ == std::nullopt || last_->part != Part::End)
    {
        if (!next_line(lines_))
        {
            bad_file(lines_, "the file ends without " + std::string(word_of(Part::End)));
        }
        section();
    }
    rest();
    return std::move(model_);
}

void NativeReader::section()
{
    Record header(lines_);
    const std::string_view word = header.word();
    const std::optional<Part> part = native::part_of(word);
    if (!part)
    {
        header.refuse(quote(word) + " is not a section");
    }
    const std::string where = lines_.where();
    try
    {
        apply(header, *part, std::string(word));
    }
    catch (const Error &error)
    {
        reject(where, error);
    }
}

void NativeReader::enter(Key key, const std::string &word)
{
    if (last_ && key <= *last_)
    {
        bad_file(lines_, "section " + word + " is out of order");
    }
    last_ = key;
}

void NativeReader::apply(Record &header, Part part, const std::string &word)
{
    if (part == Part::End)
    {
        header.end();
        enter(Key{.part = part, .family = 0}, word);
        return;
    }
    if (part == Part::Name)
    {
        std::string name = header.name();
        header.end();
        enter(Key{.part = part, .family = 0}, word);
        model_.set_name(name);
        return;
    }
    const bool named = part == Part::Names || part == Part::Tolerances;
    const Family family = named ? family_of(header) : Family::Variable;
    const Offset count = header.count();
    header.end();
    const std::string title = named ? word + " " + std::string(noun_of(family)) : word;
    enter(Key{.part = part, .family = static_cast<std::size_t>(family)}, title);
    Body body(lines_, title, count);
    fill(body, part, family);
}

void NativeReader::fill(Body &body, Part part, Family family)
{
    switch (part)
    {
    case Part::Name:
    case Part::End:
        return;
    case Part::Variables:
        native::read_variables(body, model_);
        return;
    case Part::Functions:
        native::read_functions(body, model_);
        return;
    case Part::Linear:
        native::read_linear(body, model_);
        return;
    case Part::Quadratic:
        native::read_quadratic(body, model_);
        return;
    case Part::Piecewise:
        native::read_piecewise(body, model_);
        return;
    case Part::Expressions:
        native::read_expressions(body, model_);
        return;
    case Part::Constraints:
        native::read_constraints(body, model_);
        return;
    case Part::Objectives:
        native::read_objectives(body, model_);
        return;
    case Part::Indicators:
        native::read_indicators(body, model_);
        return;
    case Part::Sets:
        native::read_sets(body, model_);
        return;
    case Part::Cones:
        native::read_cones(body, model_);
        return;
    case Part::Complements:
        native::read_complements(body, model_);
        return;
    case Part::Names:
        native::read_names(body, model_, family);
        return;
    case Part::Tolerances:
        native::read_tolerances(body, model_, family);
        return;
    }
}

void NativeReader::rest()
{
    if (next_line(lines_))
    {
        bad_file(lines_, "a line follows " + std::string(word_of(Part::End)));
    }
}

}

Model read_native(Lines &lines)
{
    NativeReader reader(lines, "the model");
    return reader.read();
}

Model read_native(std::istream &input, const std::string &file)
{
    Lines lines(input, file);
    NativeReader reader(lines, "the file");
    return reader.read();
}

}
