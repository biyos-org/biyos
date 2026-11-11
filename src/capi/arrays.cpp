#include "capi/arrays.hpp"

#include "capi/entry.hpp"
#include "core/error.hpp"
#include "core/index.hpp"

#include <cstddef>
#include <span>
#include <string>

namespace biyos::capi
{

std::size_t length(int count)
{
    if (count < 0)
    {
        const std::string reason = std::to_string(count) + " is negative";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "argument 'count'", reason);
    }
    return static_cast<std::size_t>(count);
}

std::span<const Index> listed(int count, const int *indices)
{
    const std::size_t size = length(count);
    if (size > 0)
    {
        require(indices, "indices");
    }
    return {indices, size};
}

}
