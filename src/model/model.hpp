#pragma once

#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/uses.hpp"
#include "model/attributes/names.hpp"
#include "model/attributes/overrides.hpp"
#include "model/expressions/callbacks.hpp"

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
    Continuous = BIYOS_VARKND_CONT,
    Binary = BIYOS_VARKND_BIN,
    Integer = BIYOS_VARKND_INT,
    SemiContinuous = BIYOS_VARKND_SEMICONT,
    SemiInteger = BIYOS_VARKND_SEMIINT,
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

struct Constraint
{
    Index function;
    double lower;
    double upper;
};

enum class ObjectiveSense
{
    Minimize = BIYOS_OBJSENSE_MIN,
    Maximize = BIYOS_OBJSENSE_MAX,
};

inline constexpr std::array kSenses{
    ObjectiveSense::Minimize,
    ObjectiveSense::Maximize,
};

struct Objective
{
    Index function;
    ObjectiveSense sense;
    int priority;
    double weight;
    double absolute;
    double relative;
};

Objective default_objective(Index function, Index index) noexcept;

struct Indicator
{
    Index variable;
    int value;
    Index function;
    double lower;
    double upper;
};

enum class SpecialOrderedSetKind
{
    One = BIYOS_SOSKND_ONE,
    Two = BIYOS_SOSKND_TWO,
};

inline constexpr std::array kSpecialOrderedSetKinds{
    SpecialOrderedSetKind::One,
    SpecialOrderedSetKind::Two,
};

struct Member
{
    Index variable;
    double weight;
};

enum class ConeKind
{
    Nonnegative = BIYOS_CONEKND_NONNEG,
    SecondOrder = BIYOS_CONEKND_SOC,
    Rotated = BIYOS_CONEKND_RSOC,
    Exponential = BIYOS_CONEKND_EXP,
    DualExponential = BIYOS_CONEKND_DEXP,
    Power = BIYOS_CONEKND_POW,
    DualPower = BIYOS_CONEKND_DPOW,
    Semidefinite = BIYOS_CONEKND_PSD,
};

inline constexpr std::array kConeKinds{
    ConeKind::Nonnegative,
    ConeKind::SecondOrder,
    ConeKind::Rotated,
    ConeKind::Exponential,
    ConeKind::DualExponential,
    ConeKind::Power,
    ConeKind::DualPower,
    ConeKind::Semidefinite,
};

struct Complement
{
    Index function;
    double lower;
    double upper;
    Index multiplier;
};

enum class Family
{
    Variable,
    Function,
    Constraint,
    Objective,
    Indicator,
    SpecialOrderedSet,
    Cone,
    Complement,
};

inline constexpr std::array kFamilies{
    Family::Variable,
    Family::Function,
    Family::Constraint,
    Family::Objective,
    Family::Indicator,
    Family::SpecialOrderedSet,
    Family::Cone,
    Family::Complement,
};

std::string_view noun_of(Family family) noexcept;

inline constexpr Offset kNoRoot = -1;

struct Postfix;

class Model
{
  public:
    const std::string *name() const noexcept;
    void set_name(std::string_view name);
    void delete_name() noexcept;

    Index n_of(Family family) const noexcept;
    const std::string *name_of(Family family, Index index) const;
    Index index_of(Family family, std::string_view name) const;
    void set_names(Family family,
                   std::span<const Index> indices,
                   std::span<const std::string_view> names);
    void delete_names(Family family, std::span<const Index> indices);

    Override tolerance_of(Family family, Index index) const;
    void set_tolerances(Family family,
                        std::span<const Index> indices,
                        std::span<const double> absolute,
                        std::span<const double> relative);
    void delete_tolerances(Family family, std::span<const Index> indices);

    Index n_variables() const noexcept;
    const Variable &variable(Index index) const;
    std::span<const Variable> variables(Index start, Index size) const;

    void add_variables(std::span<const Variable> added);
    void set_variables(std::span<const Index> indices, std::span<const Variable> changed);
    std::vector<Index> delete_variables(std::span<const Index> indices);

    Index n_functions() const noexcept;
    const double &constant(Index index) const;
    std::span<const double> constants(Index start, Index size) const;
    std::span<const LinearTerm> linear_terms(Index start, Index size) const;

    void check_room(std::size_t functions, Offset terms) const;
    void add_functions(std::span<const double> constants,
                       std::span<const Offset> starts,
                       std::vector<LinearTerm> terms);
    void set_constants(std::span<const Index> indices, std::span<const double> values);
    void add_constants(std::span<const Index> indices, std::span<const double> values);
    void add_linear_terms(std::span<const Index> functions,
                          std::span<const Offset> starts,
                          std::vector<LinearTerm> terms);
    std::vector<Index> delete_functions(std::span<const Index> indices);

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
    std::vector<Index> delete_constraints(std::span<const Index> indices);

    Index n_objectives() const noexcept;
    const Objective &objective(Index index) const;
    std::span<const Objective> objectives(Index start, Index size) const;

    void check_objective_room(std::size_t added) const;
    void add_objectives(std::span<const Objective> added);
    void set_objectives(std::span<const Index> indices, std::span<const Objective> changed);
    std::vector<Index> delete_objectives(std::span<const Index> indices);

    Index n_indicators() const noexcept;
    const Indicator &indicator(Index index) const;
    std::span<const Indicator> indicators(Index start, Index size) const;

    void add_indicators(std::span<const Indicator> added);
    void set_indicators(std::span<const Index> indices, std::span<const Indicator> changed);
    std::vector<Index> delete_indicators(std::span<const Index> indices);

    Index n_special_ordered_sets() const noexcept;
    SpecialOrderedSetKind kind_of_special_ordered_set(Index index) const;
    std::span<const Member> members(Index start, Index size) const;

    void check_special_ordered_set_room(std::size_t soss, Offset members) const;
    void add_special_ordered_sets(std::span<const SpecialOrderedSetKind> kinds,
                                  std::span<const Offset> starts,
                                  std::span<const Member> members);
    std::vector<Index> delete_special_ordered_sets(std::span<const Index> indices);

    Index n_cones() const noexcept;
    std::span<const ConeKind> cone_kinds(Index start, Index size) const;
    std::span<const Index> cone_functions(Index start, Index size) const;
    std::span<const double> cone_parameters(Index start, Index size) const;

    void check_cone_room(std::size_t cones, Offset members, Offset parameters) const;
    void add_cones(std::span<const ConeKind> kinds,
                   std::span<const Offset> starts,
                   std::span<const Index> functions,
                   std::span<const Offset> parameter_starts,
                   std::span<const double> parameters);
    std::vector<Index> delete_cones(std::span<const Index> indices);

    Index n_complements() const noexcept;
    const Complement &complement(Index index) const;
    std::span<const Complement> complements(Index start, Index size) const;

    void add_complements(std::span<const Complement> added);
    void set_complements(std::span<const Index> indices, std::span<const Complement> changed);
    std::vector<Index> delete_complements(std::span<const Index> indices);

    const Graph &graph() const noexcept;
    std::span<const Offset> roots(Index start, Index size) const;

    Offset add_variable_node(Index variable);
    Offset add_node(int op, std::span<const Offset> children, std::span<const double> values);
    std::vector<Offset> add_expressions(const Postfix &postfix);
    void attach_expressions(std::span<const Index> functions, std::span<const Offset> nodes);
    void clear_functions(std::span<const Index> functions);

    Index n_callbacks() const noexcept;
    const Callback &callback(Index index) const;
    Index add_callback(std::span<const Index> functions, void *data);
    void set_callback_function(Index callback, UserFunction function);
    void set_callback_jacobian(Index callback,
                               std::span<const Index> functions,
                               std::span<const Index> variables,
                               UserFunction jacobian);
    void set_callback_hessian(Index callback,
                              std::span<const Index> first,
                              std::span<const Index> second,
                              UserFunction hessian);
    void set_callback_hessian_product(Index callback, UserFunction hessian_product);
    void set_callback_data(Index callback, void *data);

  private:
    void renumber_names(Family family, std::span<const Index> map) noexcept;
    void renumber_tolerances(Family family, std::span<const Index> map) noexcept;
    void check_kinds(std::span<const Index> indices, std::span<const Variable> changed) const;
    void check_unused(std::span<const Index> map) const;
    void renumber_functions(std::span<const Index> map) noexcept;
    void renumber_variables(std::span<const Index> map) noexcept;
    Offset begin_of(std::size_t function) const noexcept;
    void append(std::span<const double> constants,
                std::span<const Offset> starts,
                std::span<const LinearTerm> terms);
    Offset quadratic_begin(std::size_t function) const noexcept;
    Offset member_begin(std::size_t sos) const noexcept;
    Callback &changed(Index callback);

    std::string name_;
    std::array<Names, kFamilies.size()> names_;
    std::array<Overrides, kFamilies.size()> tolerances_;
    std::vector<Variable> variables_;
    std::vector<double> constants_;
    std::vector<Offset> linear_ends_;
    std::vector<LinearTerm> linear_terms_;
    std::vector<Offset> quadratic_ends_;
    std::vector<QuadraticTerm> quadratic_terms_;
    std::vector<Constraint> constraints_;
    std::vector<Objective> objectives_;
    std::vector<Indicator> indicators_;
    std::vector<SpecialOrderedSetKind> special_ordered_set_kinds_;
    std::vector<Offset> special_ordered_set_ends_;
    std::vector<Member> members_;
    std::vector<ConeKind> cone_kinds_;
    std::vector<Offset> cone_ends_;
    std::vector<Index> cone_functions_;
    std::vector<Offset> parameter_ends_;
    std::vector<double> cone_parameters_;
    std::vector<Complement> complements_;
    Graph graph_;
    Uses uses_;
    std::vector<Offset> roots_;
    std::vector<Callback> callbacks_;
};

}
