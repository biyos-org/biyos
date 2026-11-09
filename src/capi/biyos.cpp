#include "biyos/biyos.h"

#include "core/version.hpp"

void biyos_version(int *major, int *minor, int *patch)
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
