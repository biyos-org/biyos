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

std::string_view kind_name(ErrorKind knd) noexcept
{
    switch (knd)
    {
    case ErrorKind::InvalidArgument:
        return "invalid argument";
    case ErrorKind::NoMemory:
        return "out of memory";
    case ErrorKind::Internal:
        return "internal error";
    }
    return "unknown error";
}

Error::Error(ErrorKind knd, std::string object, std::string msg) : kind_(knd)
{
    std::string text(kind_name(knd));
    if (!object.empty())
    {
        text += ": ";
        text += object;
    }
    text += ": ";
    text += msg;
    detail_ =
        std::make_shared<const Detail>(Detail{std::move(object), std::move(msg), std::move(text)});
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
