#include "biyos/biyos.h"

#include "capi/event.hpp"
#include "capi/firewall.hpp"
#include "core/format.hpp"

#include <cstddef>
#include <string>

namespace
{

void copy_line(const std::string &line, char *buffer, std::size_t size, std::size_t *needed)
{
    if (size > 0)
    {
        const std::size_t written = line.copy(buffer, size - 1);
        buffer[written] = '\0';
    }
    *needed = line.size();
}

template <auto Formatter>
biyos_status formatted(const biyos_event *event,
                       char *buffer,
                       std::size_t size,
                       std::size_t *needed) noexcept
{
    if (event == nullptr || needed == nullptr || (buffer == nullptr && size > 0))
    {
        return BIYOS_INVALID_ARGUMENT;
    }
    return biyos::capi::guard([&] { copy_line(Formatter(*event->core), buffer, size, needed); });
}

}

biyos_status biyos_format_text(const biyos_event *event, char *buffer, size_t size, size_t *needed)
{
    return formatted<biyos::format_text>(event, buffer, size, needed);
}

biyos_status biyos_format_json(const biyos_event *event, char *buffer, size_t size, size_t *needed)
{
    return formatted<biyos::format_json>(event, buffer, size, needed);
}
