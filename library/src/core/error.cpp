#include "core/error.hpp"

#include <utility>

namespace biyos
{

struct Error::Detail
{
    std::string object;
    std::string message;
    std::string what;
};

std::string_view kind_name(ErrorKind kind) noexcept
{
    switch (kind)
    {
    case ErrorKind::InvalidArgument:
        return "invalid argument";
    case ErrorKind::BadFile:
        return "bad file";
    case ErrorKind::Unsupported:
        return "unsupported";
    case ErrorKind::Busy:
        return "busy";
    case ErrorKind::InUse:
        return "in use";
    case ErrorKind::Callback:
        return "callback";
    case ErrorKind::NoMemory:
        return "out of memory";
    case ErrorKind::FileAccess:
        return "file access";
    case ErrorKind::Internal:
        return "internal error";
    }
    return "unknown error";
}

Error::Error(ErrorKind kind, std::string object, std::string message) : kind_(kind)
{
    std::string text(kind_name(kind));
    if (!object.empty())
    {
        text += ": ";
        text += object;
    }
    text += ": ";
    text += message;
    detail_ = std::make_shared<const Detail>(
        Detail{std::move(object), std::move(message), std::move(text)});
}

ErrorKind Error::kind() const noexcept
{
    return kind_;
}

const std::string &Error::object() const noexcept
{
    return detail_->object;
}

const std::string &Error::message() const noexcept
{
    return detail_->message;
}

const char *Error::what() const noexcept
{
    return detail_->what.c_str();
}

}
