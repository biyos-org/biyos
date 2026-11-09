#include "core/version.hpp"

namespace biyos
{

Version version()
{
    return {BIYOS_VERSION_MAJOR, BIYOS_VERSION_MINOR, BIYOS_VERSION_PATCH};
}

}
