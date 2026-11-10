#pragma once

#include <exception>
#include <memory>
#include <string>
#include <string_view>

namespace biyos
{

enum class ErrorKind
{
    InvalidArgument,
    Internal,
};

std::string_view kind_name(ErrorKind kind) noexcept;

class Error : public std::exception
{
  public:
    Error(ErrorKind kind, std::string object, std::string message);

    ErrorKind kind() const noexcept;
    const std::string &object() const noexcept;
    const std::string &message() const noexcept;
    const char *what() const noexcept override;

  private:
    struct Detail;

    ErrorKind kind_;
    std::shared_ptr<const Detail> detail_;
};

}
