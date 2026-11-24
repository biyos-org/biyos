#include "biyos/biyos.h"

#include "core/version.hpp"

void BIYOS_version(BIYOS_int *major, BIYOS_int *minor, BIYOS_int *patch)
{
    const biyos::Version v = biyos::version();
    if (major != nullptr)
    {
        *major = v.major;
    }
    if (minor != nullptr)
    {
        *minor = v.minor;
    }
    if (patch != nullptr)
    {
        *patch = v.patch;
    }
}
