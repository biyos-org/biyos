#include "io/mps/mps.hpp"

#include "core/events/stream.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/mps/readers/bounds.hpp"
#include "io/mps/readers/columns.hpp"
#include "io/mps/readers/indicators.hpp"
#include "io/mps/readers/rows.hpp"
#include "io/mps/readers/sets.hpp"
#include "io/mps/readers/values.hpp"
#include "io/mps/readers/vectors.hpp"
#include "io/mps/sections.hpp"
#include "io/records.hpp"
#include "io/report.hpp"
#include "model/model.hpp"

#include <istream>
#include <optional>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

class MpsReader
{
  public:
    MpsReader(Stream stream, std::istream &input, const std::string &file);

    Model read();

  private:
    bool skipped() const noexcept;
    void header();
    void enter(Section section);
    void leave();
    void read_name();
    void read_sense(std::string_view word);
    void data();
    void rest();

    Stream stream_;
    Lines lines_;
    Draft draft_;
    Section section_ = Section::None;
    bool sensed_ = false;
    Columns columns_;
    Vectors rhs_{"RHS"};
    Vectors ranges_{"RANGES"};
    Vectors bounds_{"BOUNDS"};
    Sets sets_;
};

MpsReader::MpsReader(Stream stream, std::istream &input, const std::string &file)
    : stream_(stream), lines_(input, file)
{
}

Model MpsReader::read()
{
    while (section_ != Section::End && lines_.next())
    {
        if (skipped())
        {
            continue;
        }
        const char first = lines_.text().front();
        if (first == ' ' || first == '\t')
        {
            data();
        }
        else
        {
            header();
        }
    }
    if (lines_.number() == 0)
    {
        bad_file(lines_, "is empty");
    }
    if (section_ != Section::End)
    {
        bad_file(lines_, "the file ends without ENDATA");
    }
    rest();
    return build(draft_);
}

bool MpsReader::skipped() const noexcept
{
    return lines_.fields().empty() || lines_.text().front() == '*';
}

void MpsReader::header()
{
    const std::string_view word = lines_.fields().front();
    const std::optional<Section> section = section_of(word);
    if (!section)
    {
        unsupported(lines_, "section " + std::string(word));
    }
    enter(*section);
    if (*section == Section::Name)
    {
        read_name();
        return;
    }
    if (*section == Section::Sense && lines_.fields().size() == 2)
    {
        read_sense(lines_.fields()[1]);
        return;
    }
    if (lines_.fields().size() > 1)
    {
        bad_file(lines_, "section " + std::string(word) + " takes no field");
    }
}

void MpsReader::enter(Section section)
{
    const std::string word(word_of(section));
    if (section <= section_)
    {
        bad_file(lines_, "section " + word + " is out of order");
    }
    for (const Section needed : {Section::Name, Section::Rows, Section::Columns})
    {
        if (section_ < needed && needed < section)
        {
            bad_file(lines_, "section " + word + " comes before " + std::string(word_of(needed)));
        }
    }
    if (section_ < Section::Rhs && Section::Rhs < section)
    {
        warn(stream_, lines_, "the file has no RHS section, so every right-hand side is 0");
    }
    leave();
    section_ = section;
}

void MpsReader::leave()
{
    if (section_ == Section::Columns)
    {
        columns_.close(lines_);
    }
    if (section_ == Section::Bounds)
    {
        warn_negative(stream_, lines_, draft_);
    }
}

void MpsReader::read_name()
{
    const std::string_view name = trimmed(lines_.text().substr(word_of(Section::Name).size()));
    if (!name.empty())
    {
        check_name(lines_, name);
        draft_.name = std::string(name);
    }
}

void MpsReader::read_sense(std::string_view word)
{
    if (sensed_)
    {
        bad_file(lines_, "the objective sense is given twice");
    }
    draft_.sense = sense_of(lines_, word);
    sensed_ = true;
}

void MpsReader::data()
{
    switch (section_)
    {
    case Section::None:
        bad_file(lines_, "a data line comes before NAME");
    case Section::Name:
        bad_file(lines_, "a data line follows NAME");
    case Section::Sense:
        if (lines_.fields().size() != 1)
        {
            bad_file(lines_, "an objective sense is one word");
        }
        read_sense(lines_.fields().front());
        return;
    case Section::Rows:
        read_row(lines_, draft_);
        return;
    case Section::Columns:
        columns_.read(lines_, draft_);
        return;
    case Section::Rhs:
        read_rhs(stream_, lines_, rhs_, draft_);
        return;
    case Section::Ranges:
        read_range(stream_, lines_, ranges_, draft_);
        return;
    case Section::Bounds:
        read_bound(stream_, lines_, bounds_, draft_);
        return;
    case Section::Sets:
        sets_.read(lines_, draft_);
        return;
    case Section::Indicators:
        read_indicator(lines_, draft_);
        return;
    case Section::End:
        break;
    }
    bad_file(lines_, "a data line follows ENDATA");
}

void MpsReader::rest()
{
    while (lines_.next())
    {
        if (!skipped())
        {
            bad_file(lines_, "a line follows ENDATA");
        }
    }
}

}

Model read_mps(Stream stream, std::istream &input, const std::string &file)
{
    return MpsReader(stream, input, file).read();
}

}
