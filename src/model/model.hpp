#pragma once

#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/terms.hpp"
#include "model/names.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

enum class VariableKind
{
    Continuous = BIYOS_VAR_CONT,
    Binary = BIYOS_VAR_BIN,
    Integer = BIYOS_VAR_INT,
    SemiContinuous = BIYOS_VAR_SEMICONT,
    SemiInteger = BIYOS_VAR_SEMIINT,
};

inline constexpr std::array kVariableKinds{
    VariableKind::Continuous,
    VariableKind::Binary,
    VariableKind::Integer,
    VariableKind::SemiContinuous,
    VariableKind::SemiInteger,
};

struct Variable
{
    VariableKind kind;
    double lower;
    double upper;
};

Variable widest(VariableKind kind) noexcept;

enum class Sense
{
    Minimize = BIYOS_MINIMIZE,
    Maximize = BIYOS_MAXIMIZE,
};

inline constexpr std::array kSenses{
    Sense::Minimize,
    Sense::Maximize,
};

struct Constraint
{
    Index function;
    double lower;
    double upper;
};

struct Objective
{
    Index function;
    Sense sense;
    int priority;
    double weight;
    double absolute;
    double relative;
};

Objective default_objective(Index function, Index index) noexcept;

enum class Family
{
    Variable,
    Function,
    Constraint,
    Objective,
};

inline constexpr std::array kFamilies{
    Family::Variable,
    Family::Function,
    Family::Constraint,
    Family::Objective,
};

std::string_view noun_of(Family family) noexcept;

class Model
{
  public:
    const std::string *name() const noexcept;
    void set_name(std::string_view name);
    void delete_name() noexcept;

    const std::string *name_of(Family family, Index index) const;
    Index index_of(Family family, std::string_view name) const;
    void set_names(Family family,
                   std::span<const Index> indices,
                   std::span<const std::string_view> names);
    void delete_names(Family family, std::span<const Index> indices);

    Index n_variables() const noexcept;
    const Variable &variable(Index index) const;
    std::span<const Variable> variables(Index start, Index size) const;

    void add_variables(std::span<const Variable> added);
    void set_variables(std::span<const Index> indices, std::span<const Variable> changed);

    Index n_functions() const noexcept;
    std::span<const double> constants(Index start, Index size) const;
    std::span<const LinearTerm> linear_terms(Index start, Index size) const;

    void check_room(std::size_t functions, Offset terms) const;
    void add_functions(std::span<const double> constants,
                       std::span<const Offset> starts,
                       std::vector<LinearTerm> terms);
    void add_linear_terms(std::span<const Index> functions,
                          std::span<const Offset> starts,
                          std::vector<LinearTerm> terms);

    std::span<const QuadraticTerm> quadratic_terms(Index start, Index size) const;

    void check_quadratic_room(Offset terms) const;
    void add_quadratic_terms(std::span<const Index> functions,
                             std::span<const Offset> starts,
                             std::vector<QuadraticTerm> terms);

    Index n_constraints() const noexcept;
    const Constraint &constraint(Index index) const;
    std::span<const Constraint> constraints(Index start, Index size) const;

    void add_constraints(std::span<const Constraint> added);
    void set_constraints(std::span<const Index> indices, std::span<const Constraint> changed);

    Index n_objectives() const noexcept;
    const Objective &objective(Index index) const;
    std::span<const Objective> objectives(Index start, Index size) const;

    void check_objective_room(std::size_t added) const;
    void add_objectives(std::span<const Objective> added);
    void set_objectives(std::span<const Index> indices, std::span<const Objective> changed);

  private:
    Index n_of(Family family) const noexcept;
    Offset begin_of(std::size_t function) const noexcept;
    void append(std::span<const double> constants,
                std::span<const Offset> starts,
                std::span<const LinearTerm> terms);
    Offset quadratic_begin(std::size_t function) const noexcept;

    std::string name_;
    std::vector<Variable> variables_;
    std::vector<double> constants_;
    std::vector<Offset> ends_;
    std::vector<LinearTerm> terms_;
    std::vector<Offset> quadratic_ends_;
    std::vector<QuadraticTerm> quadratic_terms_;
    std::vector<Constraint> constraints_;
    std::vector<Objective> objectives_;
    std::array<Names, kFamilies.size()> names_;
};

}
