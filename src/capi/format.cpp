#include "biyos/biyos.h"

#include "capi/event.hpp"
#include "capi/firewall.hpp"
#include "core/format.hpp"

#include <cstddef>
#include <string>

namespace
{

void copy_line(const std::string &line, char *buffer, std::size_t capacity, std::size_t *needed)
{
    if (capacity > 0)
    {
        const std::size_t written = line.copy(buffer, capacity - 1);
        buffer[written] = '\0';
    }
    *needed = line.size();
}

template <auto Formatter>
BIYOS_status formatted(const BIYOS_event *event,
                       char *buffer,
                       std::size_t capacity,
                       std::size_t *needed) noexcept
{
    if (event == nullptr || needed == nullptr || (buffer == nullptr && capacity > 0))
    {
        return BIYOS_ERR_BADARG;
    }
    return biyos::capi::guard([&]
                              { copy_line(Formatter(*event->core), buffer, capacity, needed); });
}

}

BIYOS_status BIYOS_fmttext(const BIYOS_event *evt, size_t cap, char *buf, size_t *len)
{
    return formatted<biyos::format_text>(evt, buf, cap, len);
}

BIYOS_status BIYOS_fmtjson(const BIYOS_event *evt, size_t cap, char *buf, size_t *len)
{
    return formatted<biyos::format_json>(evt, buf, cap, len);
}
