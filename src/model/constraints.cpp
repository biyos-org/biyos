#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "constraint";

void check_bnds(Index idx, const Constraint &con)
{
    const std::string_view fault = bound_fault(con.lower, con.upper);
    if (!fault.empty())
    {
        refuse(kNoun, idx, std::string(fault));
    }
}

void check(Index idx, const Constraint &con, Index funs)
{
    check_fun(kNoun, idx, con.function, funs);
    check_bnds(idx, con);
}

}

Index Model::n_cons() const noexcept
{
    return static_cast<Index>(constraints_.size());
}

const Constraint &Model::constraint(Index idx) const
{
    check_idx(kNoun, idx, n_cons());
    return constraints_[static_cast<std::size_t>(idx)];
}

std::span<const Constraint> Model::cons(Index beg, Index n) const
{
    check_span(kNoun, beg, n, n_cons());
    const auto start = static_cast<std::size_t>(beg);
    return std::span(constraints_).subspan(start, static_cast<std::size_t>(n));
}

void Model::add_cons(std::span<const Constraint> added)
{
    check_n(kNoun, constraints_, added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(static_cast<Index>(constraints_.size() + k), added[k], n_funs());
    }
    constraints_.insert(constraints_.end(), added.begin(), added.end());
}

void Model::set_cons(std::span<const Index> idx, std::span<const Constraint> changed)
{
    if (idx.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and constraints differ in number");
    }
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        check_idx(kNoun, idx[k], n_cons());
        check(idx[k], changed[k], n_funs());
    }
    for (std::size_t k = 0; k < idx.size(); ++k)
    {
        constraints_[static_cast<std::size_t>(idx[k])] = changed[k];
    }
}

}
