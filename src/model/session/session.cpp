#include "model/session/session.hpp"

#include <utility>

namespace biyos
{

Session::Session(Env &env) : handle_(env), settings_(env.options().table()) {}

Session::Session(Env &env, const Session &source)
    : handle_(env), model_(source.model_), settings_(source.settings_)
{
}

const Model &Session::model() const noexcept
{
    return model_;
}

Model &Session::change()
{
    evaluation_.reset();
    return model_;
}

Settings &Session::settings() noexcept
{
    return settings_;
}

const Settings &Session::settings() const noexcept
{
    return settings_;
}

Lookup Session::lookup() const
{
    return Lookup(settings_, env().options());
}

Hooks &Session::hooks() const noexcept
{
    return hooks_;
}

int Session::add_hook(EventKind kind, EventLevel level, Hook hook)
{
    return hooks_.add_hook(kind, level, std::move(hook));
}

void Session::delete_hook(int id)
{
    hooks_.delete_hook(id);
}

bool Session::evaluating() const noexcept
{
    return evaluating_;
}

void Session::set_evaluating(bool evaluating) noexcept
{
    evaluating_ = evaluating;
}

Env &Session::env() const noexcept
{
    return handle_.env();
}

Usage &Session::usage() const noexcept
{
    return usage_;
}

}
