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

Variable widest(VariableKind knd) noexcept;

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

Objective default_obj(Index fun, Index idx) noexcept;

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
    void del_name() noexcept;

    const std::string *name_of(Family family, Index idx) const;
    Index idx_of(Family family, std::string_view name) const;
    void set_names(Family family,
                   std::span<const Index> idx,
                   std::span<const std::string_view> names);
    void del_names(Family family, std::span<const Index> idx);

    Index n_vars() const noexcept;
    const Variable &variable(Index idx) const;
    std::span<const Variable> vars(Index beg, Index n) const;

    void add_vars(std::span<const Variable> added);
    void set_vars(std::span<const Index> idx, std::span<const Variable> changed);

    Index n_funs() const noexcept;
    std::span<const double> constants(Index beg, Index n) const;
    std::span<const LinearTerm> terms(Index beg, Index n) const;

    void check_room(std::size_t funs, Offset terms) const;
    void add_funs(std::span<const double> constants,
                  std::span<const Offset> starts,
                  std::vector<LinearTerm> terms);
    void add_lin_terms(std::span<const Index> funs,
                       std::span<const Offset> starts,
                       std::vector<LinearTerm> terms);

    std::span<const QuadraticTerm> quad_terms(Index beg, Index n) const;

    void check_quad_room(Offset terms) const;
    void add_quad_terms(std::span<const Index> funs,
                        std::span<const Offset> starts,
                        std::vector<QuadraticTerm> terms);

    Index n_cons() const noexcept;
    const Constraint &constraint(Index idx) const;
    std::span<const Constraint> cons(Index beg, Index n) const;

    void add_cons(std::span<const Constraint> added);
    void set_cons(std::span<const Index> idx, std::span<const Constraint> changed);

    Index n_objs() const noexcept;
    const Objective &objective(Index idx) const;
    std::span<const Objective> objs(Index beg, Index n) const;

    void check_obj_room(std::size_t added) const;
    void add_objs(std::span<const Objective> added);
    void set_objs(std::span<const Index> idx, std::span<const Objective> changed);

  private:
    Index n_of(Family family) const noexcept;
    Offset begin_of(std::size_t fun) const noexcept;
    void append(std::span<const double> constants,
                std::span<const Offset> starts,
                std::span<const LinearTerm> terms);
    Offset quad_begin(std::size_t fun) const noexcept;

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
