#pragma once

#include "biyos/biyos.h"
#include "capi/utils/codes.hpp"
#include "core/error.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"

#include <concepts>
#include <exception>
#include <functional>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace biyos::capi
{

inline void describe_failure(Event &event, BIYOS_status status, std::string_view text)
{
    event.module = "capi";
    event.text.assign(text);
    event.fields.push_back(Field{.name = "code", .value = std::to_string(status)});
}

inline void announce(Stream stream, ErrorKind kind, std::string_view text) noexcept
{
    const BIYOS_status status = status_of(kind);
    const auto fill = [&](Event &event) { describe_failure(event, status, text); };
    stream.emit(EventKind::Message, EventLevel::Error, fill);
}

inline void report(Stream stream, ErrorKind kind, std::string_view text) noexcept
{
    stream.record(kind, text);
    if (!stream.delivering())
    {
        announce(stream, kind, text);
    }
}

template <typename Report>
concept reporter = std::invocable<Report &, ErrorKind, std::string_view>;

template <reporter Report>
BIYOS_status fail(Report &report, ErrorKind kind, std::string_view message) noexcept
{
    try
    {
        report(kind, Error(kind, "", std::string(message)).what());
    }
    catch (...)
    {
        report(kind, kind_name(kind));
    }
    return status_of(kind);
}

template <std::invocable Body, reporter Report>
BIYOS_status trap(Body &&body, Report report) noexcept
{
    try
    {
        body();
        return BIYOS_OK;
    }
    catch (const Error &error)
    {
        report(error.kind(), error.what());
        return status_of(error.kind());
    }
    catch (const std::bad_alloc &)
    {
        report(ErrorKind::NoMemory, kind_name(ErrorKind::NoMemory));
        return status_of(ErrorKind::NoMemory);
    }
    catch (const std::exception &error)
    {
        return fail(report, ErrorKind::Internal, error.what());
    }
    catch (...)
    {
        return fail(report, ErrorKind::Internal, "unknown exception");
    }
}

template <std::invocable Body> BIYOS_status guard(Stream stream, Body &&body) noexcept
{
    return trap(std::forward<Body>(body), std::bind_front(report, stream));
}

inline void unreported(ErrorKind, std::string_view) noexcept {}

template <std::invocable Body> BIYOS_status guard(Body &&body) noexcept
{
    return trap(std::forward<Body>(body), unreported);
}

}
