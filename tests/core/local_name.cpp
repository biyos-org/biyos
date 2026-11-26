#include "core/text/name.hpp"

#include <cstddef>

using biyos::StaticName;

std::size_t local_name()
{
    char buffer[16] = "nlp";
    const StaticName name(buffer);
    return name.text().size();
}
