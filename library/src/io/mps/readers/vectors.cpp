#include "io/mps/readers/vectors.hpp"

#include "core/events/stream.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <string>
#include <string_view>

namespace biyos
{

Vectors::Vectors(std::string_view section) : section_(section) {}

bool Vectors::keeps(Stream stream, const Lines &lines, std::string_view name)
{
    if (!kept_)
    {
        kept_ = std::string(name);
    }
    if (*kept_ == name)
    {
        return true;
    }
    if (skipped_.emplace(name).second)
    {
        warn(stream,
             lines,
             section_ + " vector " + quote(name) + " is ignored, only " + quote(*kept_) +
                 " is read");
    }
    return false;
}

}
