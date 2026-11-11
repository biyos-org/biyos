#include "model/compressed.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"

#include <cstddef>
#include <span>
#include <string>

namespace biyos
{

namespace
{

[[noreturn]] void refuse_starts(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "argument 'starts'", reason);
}

}

void check_starts(std::span<const Offset> starts)
{
    if (!starts.empty() && starts.front() != 0)
    {
        refuse_starts("begins at " + text_of(starts.front()) + ", not 0");
    }
    for (std::size_t k = 1; k < starts.size(); ++k)
    {
        if (starts[k] < starts[k - 1])
        {
            const std::string fall = text_of(starts[k - 1]) + " to " + text_of(starts[k]);
            refuse_starts("decreases from " + fall + " at entry " + text_of(k));
        }
    }
}

void check_shape(std::size_t functions, std::span<const Offset> starts, std::size_t terms)
{
    if (starts.size() != functions + 1)
    {
        throw Error(ErrorKind::Internal, "model", "functions and starts differ in number");
    }
    check_starts(starts);
    if (starts.back() != static_cast<Offset>(terms))
    {
        throw Error(ErrorKind::Internal, "model", "starts and terms differ in number");
    }
}

}
