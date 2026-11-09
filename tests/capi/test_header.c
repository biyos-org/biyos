#include "biyos/biyos.h"

int main(void)
{
    int major = -1;
    int minor = -1;
    int patch = -1;
    biyos_version(&major, &minor, &patch);
    return (major >= 0 && minor >= 0 && patch >= 0) ? 0 : 1;
}
