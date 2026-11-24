#include "core/text/name.hpp"

#include <cstddef>

std::size_t local_name()
{
    char buffer[16] = "nlp";
    const biyos::StaticName name(buffer);
    return name.text().size();
}
