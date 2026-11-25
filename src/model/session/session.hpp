#pragma once

#include "core/env.hpp"
#include "core/events/event.hpp"
#include "core/events/hooks.hpp"
#include "core/memory/usage.hpp"
#include "core/options/lookup.hpp"
#include "core/options/settings.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/model.hpp"

#include <memory>

namespace biyos
{

class Session
{
  public:
    explicit Session(Env &env);
    Session(Env &env, const Session &source);
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    const Model &model() const noexcept;
    Model &change();
    Settings &settings() noexcept;
    const Settings &settings() const noexcept;
    Lookup lookup() const;
    Hooks &hooks() const noexcept;
    int add_hook(EventKind kind, EventLevel level, Hook hook);
    void delete_hook(int id);
    bool evaluating() const noexcept;
    void set_evaluating(bool evaluating) noexcept;
    Env &env() const noexcept;
    Usage &usage() const noexcept;

  private:
    Env::Handle handle_;
    Model model_;
    Settings settings_;
    mutable Usage usage_;
    mutable Hooks hooks_;
    std::unique_ptr<Evaluation> evaluation_;
    bool evaluating_ = false;
};

}
