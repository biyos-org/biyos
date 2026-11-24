#include "io/mps/vectors.hpp"

#include "core/env.hpp"
#include "io/lines.hpp"
#include "io/records.hpp"
#include "io/report.hpp"

#include <string>
#include <string_view>

namespace biyos
{

Vectors::Vectors(std::string_view section) : section_(section) {}

bool Vectors::keeps(Env &env, const Lines &lines, std::string_view name)
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
        warn(env,
             lines,
             section_ + " vector " + quote(name) + " is ignored, only " + quote(*kept_) +
                 " is read");
    }
    return false;
}

}
