#pragma once

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "io/mps/mps.hpp"
#include "model/model.hpp"

#include <initializer_list>
#include <limits>
#include <numeric>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::test
{

inline constexpr Real kInfinity = std::numeric_limits<Real>::infinity();

struct Warning
{
    std::string text;
    std::string file;
    std::string line;
};

inline std::string text(std::initializer_list<std::string_view> lines)
{
    std::string joined;
    for (const std::string_view line : lines)
    {
        joined.append(line).append("\n");
    }
    return joined;
}

inline std::string bad(int line, std::string_view message)
{
    return "bad file: file 't.mps' line " + std::to_string(line) + ": " + std::string(message);
}

inline Model read_text(Env &env, std::string_view text, MpsForm form = MpsForm::Free)
{
    std::istringstream input{std::string(text)};
    return read_mps(env.stream(), input, "t.mps", form);
}

inline Model read_sample(std::string_view text, MpsForm form = MpsForm::Free)
{
    Env env;
    return read_text(env, text, form);
}

inline std::string fault_of(std::string_view text, MpsForm form = MpsForm::Free)
{
    Env env;
    try
    {
        read_text(env, text, form);
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "";
}

inline Warning warning_of(const Event &event)
{
    return Warning{
        .text = event.text,
        .file = event.fields.at(0).value,
        .line = event.fields.at(1).value,
    };
}

inline std::vector<Warning> warnings_of(std::string_view text, MpsForm form = MpsForm::Free)
{
    Env env;
    std::vector<Warning> heard;
    const auto record = [&heard](const Event &event) { heard.push_back(warning_of(event)); };
    env.events().add_hook(EventKind::Message, EventLevel::Warning, record);
    read_text(env, text, form);
    return heard;
}

inline std::vector<Index> indices(std::size_t n)
{
    std::vector<Index> all(n);
    std::iota(all.begin(), all.end(), Index{0});
    return all;
}

inline void rename(Model &model, Family family, const std::vector<std::string_view> &names)
{
    model.set_names(family, indices(names.size()), names);
}

inline void add_columns(Model &model,
                        const std::vector<Variable> &variables,
                        const std::vector<std::string_view> &names)
{
    model.add_variables(variables);
    rename(model, Family::Variable, names);
}

inline void add_rows(Model &model,
                     const std::vector<Real> &constants,
                     const std::vector<Offset> &starts,
                     std::vector<LinearTerm> terms)
{
    model.add_functions(constants, starts, std::move(terms));
}

inline void add_named(Model &model,
                      const std::vector<Constraint> &constraints,
                      const std::vector<std::string_view> &names)
{
    model.add_constraints(constraints);
    rename(model, Family::Constraint, names);
}

inline void add_goal(Model &model, Index function, ObjectiveSense sense, std::string_view name)
{
    Objective objective = default_objective(function, 0);
    objective.sense = sense;
    model.add_objectives(std::span(&objective, 1));
    const Index index = 0;
    model.set_names(Family::Objective, std::span(&index, 1), std::span(&name, 1));
}

inline Variable continuous(Real lower, Real upper)
{
    return Variable{.kind = VariableKind::Continuous, .lower = lower, .upper = upper};
}

}
