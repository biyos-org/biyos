#include "io/mps/readers/bounds.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "core/numeric/zero.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/mps/readers/vectors.hpp"
#include "io/records.hpp"
#include "io/report.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{
enum class Effect
{
    None,
    Value,
    Zero,
    One,
    MinusInfinity,
    PlusInfinity,
};

enum class Need
{
    Value,
    Integral,
    One,
    Nothing,
};

struct BoundKind
{
    std::string_view code;
    Effect lower;
    Effect upper;
    Need need;
    bool integer;
    bool semi;
    bool binary;
};

constexpr std::array kBoundKinds{
    BoundKind{"LO", Effect::Value, Effect::None, Need::Value, false, false, false},
    BoundKind{"UP", Effect::None, Effect::Value, Need::Value, false, false, false},
    BoundKind{"FX", Effect::Value, Effect::Value, Need::Value, false, false, false},
    BoundKind{"FR",
              Effect::MinusInfinity,
              Effect::PlusInfinity,
              Need::Nothing,
              false,
              false,
              false},
    BoundKind{"MI", Effect::MinusInfinity, Effect::None, Need::Nothing, false, false, false},
    BoundKind{"PL", Effect::None, Effect::PlusInfinity, Need::Nothing, false, false, false},
    BoundKind{"BV", Effect::Zero, Effect::One, Need::One, false, false, true},
    BoundKind{"LI", Effect::Value, Effect::None, Need::Integral, true, false, false},
    BoundKind{"UI", Effect::None, Effect::Value, Need::Integral, true, false, false},
    BoundKind{"SC", Effect::None, Effect::Value, Need::Value, false, true, false},
    BoundKind{"SI", Effect::None, Effect::Value, Need::Value, true, true, false},
};

const BoundKind &kind_of(const Lines &lines, std::string_view code)
{
    const auto found = std::ranges::find(kBoundKinds, code, &BoundKind::code);
    if (found == kBoundKinds.end())
    {
        bad_file(lines, "unknown bound type " + quote(code));
    }
    return *found;
}

bool integral(Real value) noexcept
{
    Real whole = 0.0;
    return is_zero(std::modf(value, &whole));
}

std::optional<Real> checked_value(Stream stream,
                                  const Lines &lines,
                                  const BoundKind &kind,
                                  Fields fields)
{
    const bool given = fields.size() == 4;
    const std::string type = std::string(kind.code) + " bound";
    if (!given)
    {
        if (kind.need == Need::Value || kind.need == Need::Integral)
        {
            bad_file(lines, type + " needs a value");
        }
        return std::nullopt;
    }
    if (kind.need == Need::Nothing)
    {
        warn(stream, lines, type + " takes no value, " + quote(fields[3]) + " is ignored");
        return std::nullopt;
    }
    const Real value = value_of(lines, fields[3]);
    if (kind.need == Need::Integral && !integral(value))
    {
        bad_file(lines, type + " value " + quote(fields[3]) + " is not an integer");
    }
    if (kind.need == Need::One && !is_zero(value - 1.0))
    {
        bad_file(lines, type + " value " + quote(fields[3]) + " is not 1");
    }
    return value;
}

Real resolve(Effect effect, std::optional<Real> value) noexcept
{
    switch (effect)
    {
    case Effect::Value:
        return value.value_or(0.0);
    case Effect::Zero:
    case Effect::None:
        return 0.0;
    case Effect::One:
        return 1.0;
    case Effect::MinusInfinity:
        return -kInfinity;
    case Effect::PlusInfinity:
        break;
    }
    return kInfinity;
}

void apply(const Lines &lines,
           std::optional<Real> &slot,
           Effect effect,
           std::optional<Real> value,
           std::string_view what)
{
    if (effect == Effect::None)
    {
        return;
    }
    if (slot)
    {
        const std::string column = quote(lines.fields()[2]);
        bad_file(lines, "column " + column + " has a second " + std::string(what) + " bound");
    }
    slot = resolve(effect, value);
}

}

void read_bound(Stream stream, const Lines &lines, Vectors &vectors, Draft &draft)
{
    const Fields fields = uncommented(lines.fields(), 2);
    if (fields.size() != 3 && fields.size() != 4)
    {
        bad_file(lines, "a bound needs a type, a vector, a column and a value");
    }
    const BoundKind &kind = kind_of(lines, fields[0]);
    if (!vectors.keeps(stream, lines, fields[1]))
    {
        return;
    }
    const Index index = find_in(draft.column_names, lines, "column", fields[2]);
    const std::optional<Real> value = checked_value(stream, lines, kind, fields);
    DraftColumn &column = draft.columns[to_size(index)];
    apply(lines, column.lower, kind.lower, value, "lower");
    apply(lines, column.upper, kind.upper, value, "upper");
    if (kind.upper != Effect::None)
    {
        column.upper_line = lines.number();
    }
    column.integer = column.integer || kind.integer;
    column.semi = column.semi || kind.semi;
    column.binary = column.binary || kind.binary;
}

void warn_negative(Stream stream, const Lines &lines, const Draft &draft)
{
    for (std::size_t k = 0; k < draft.columns.size(); ++k)
    {
        const DraftColumn &column = draft.columns[k];
        if (negative_upper(column))
        {
            const std::string name = quote(draft.column_names.names()[k]);
            warn(stream,
                 lines.file(),
                 column.upper_line,
                 "column " + name +
                     " has a negative upper bound and no lower bound, so its lower bound is minus "
                     "infinity");
        }
    }
}

}
