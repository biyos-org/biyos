#include "core/name.hpp"

#include <cstddef>

std::size_t local_name()
{
    char buffer[16] = "nlp";
    const biyos::FixedName name(buffer);
    return name.text().size();
}
