#pragma once

#include "biyos/biyos.h"
#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/terms.hpp"
#include "expr/uses.hpp"
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

struct Indicator
{
    Index variable;
    int value;
    Index function;
    double lower;
    double upper;
};

enum class SosKind
{
    Sos1 = BIYOS_SOSKND_SOS1,
    Sos2 = BIYOS_SOSKND_SOS2,
};

inline constexpr std::array kSosKinds{
    SosKind::Sos1,
    SosKind::Sos2,
};

struct Member
{
    Index variable;
    double weight;
};

enum class ConeKind
{
    Nonnegative = BIYOS_CONE_NONNEG,
    SecondOrder = BIYOS_CONE_SOC,
    Rotated = BIYOS_CONE_RSOC,
    Exponential = BIYOS_CONE_EXP,
    DualExponential = BIYOS_CONE_DEXP,
    Power = BIYOS_CONE_POW,
    DualPower = BIYOS_CONE_DPOW,
    Semidefinite = BIYOS_CONE_PSD,
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

enum class Family
{
    Variable,
    Function,
    Constraint,
    Objective,
    Indicator,
    Sos,
    Cone,
};

inline constexpr std::array kFamilies{
    Family::Variable,
    Family::Function,
    Family::Constraint,
    Family::Objective,
    Family::Indicator,
    Family::Sos,
    Family::Cone,
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

    Index n_variables() const noexcept;
    const Variable &variable(Index index) const;
    std::span<const Variable> variables(Index start, Index size) const;

    void add_variables(std::span<const Variable> added);
    void set_variables(std::span<const Index> indices, std::span<const Variable> changed);

    Index n_functions() const noexcept;
    std::span<const double> constants(Index start, Index size) const;
    const double &constant(Index index) const;
    void set_constants(std::span<const Index> indices, std::span<const double> values);
    void add_constants(std::span<const Index> indices, std::span<const double> values);
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

    const Graph &graph() const noexcept;
    std::span<const Offset> roots(Index start, Index size) const;

    Offset add_variable_node(Index variable);
    Offset add_node(int op, std::span<const Offset> children, std::span<const double> values);
    std::vector<Offset> add_expressions(const Postfix &postfix);
    void attach_expressions(std::span<const Index> functions, std::span<const Offset> nodes);
    void clear_functions(std::span<const Index> functions);

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

    Index n_indicators() const noexcept;
    const Indicator &indicator(Index index) const;
    std::span<const Indicator> indicators(Index start, Index size) const;

    void add_indicators(std::span<const Indicator> added);
    void set_indicators(std::span<const Index> indices, std::span<const Indicator> changed);
    Index n_sos() const noexcept;
    SosKind kind_of_sos(Index index) const;
    std::span<const Member> members(Index start, Index size) const;

    void check_sos_room(std::size_t sos, Offset members) const;
    void add_sos(std::span<const SosKind> kinds,
                 std::span<const Offset> starts,
                 std::span<const Member> members);
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

  private:
    Offset begin_of(std::size_t function) const noexcept;
    void append(std::span<const double> constants,
                std::span<const Offset> starts,
                std::span<const LinearTerm> terms);
    Offset quadratic_begin(std::size_t function) const noexcept;
    void check_kinds(std::span<const Index> indices, std::span<const Variable> changed) const;
    Offset member_begin(std::size_t sos) const noexcept;

    std::string name_;
    std::vector<Variable> variables_;
    std::vector<double> constants_;
    std::vector<Offset> ends_;
    std::vector<LinearTerm> terms_;
    std::vector<Offset> quadratic_ends_;
    std::vector<QuadraticTerm> quadratic_terms_;
    Graph graph_;
    Uses uses_;
    std::vector<Offset> roots_;
    std::vector<Constraint> constraints_;
    std::vector<Objective> objectives_;
    std::vector<Indicator> indicators_;
    std::vector<SosKind> sos_kinds_;
    std::vector<Offset> sos_ends_;
    std::vector<Member> members_;
    std::vector<ConeKind> cone_kinds_;
    std::vector<Offset> cone_ends_;
    std::vector<Index> cone_functions_;
    std::vector<Offset> parameter_ends_;
    std::vector<double> cone_parameters_;
    std::array<Names, kFamilies.size()> names_;
};

}
