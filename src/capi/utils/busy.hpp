#pragma once

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/memory/usage.hpp"

#include <string>
#include <string_view>

namespace biyos::capi
{

inline constexpr std::string_view kChanging = "is being changed on another thread";
inline constexpr std::string_view kInside = "is in a call on another thread";

[[noreturn]] inline void refuse(std::string_view object, std::string_view reason)
{
    throw Error(ErrorKind::Busy, std::string(object), std::string(reason));
}

class EnvGuard
{
  public:
    explicit EnvGuard(Env &env) noexcept : usage_(env.usage()), entered_(usage_.enter()) {}

    ~EnvGuard()
    {
        if (entered_)
        {
            usage_.leave();
        }
    }

    EnvGuard(const EnvGuard &) = delete;
    EnvGuard &operator=(const EnvGuard &) = delete;

    void check() const
    {
        if (!entered_)
        {
            refuse("environment", kChanging);
        }
    }

    static void claim(Env &env)
    {
        if (!env.usage().claim())
        {
            refuse("environment", kInside);
        }
    }

  private:
    Usage &usage_;
    bool entered_;
};

class ModelGuard
{
  public:
    ModelGuard(Env &env, Usage &usage) noexcept : env_(env), usage_(usage), entered_(usage_.enter())
    {
    }

    ~ModelGuard()
    {
        if (entered_)
        {
            usage_.leave();
        }
    }

    ModelGuard(const ModelGuard &) = delete;
    ModelGuard &operator=(const ModelGuard &) = delete;

    void check() const
    {
        env_.check();
        if (!entered_)
        {
            refuse("model", kChanging);
        }
    }

    static void claim(Usage &usage)
    {
        if (!usage.claim())
        {
            refuse("model", kInside);
        }
    }

  private:
    EnvGuard env_;
    Usage &usage_;
    bool entered_;
};

}
