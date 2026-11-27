#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/intervals/interval.hpp"

#include <array>
#include <cstddef>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace biyos::test
{

constexpr std::size_t kVariables = 3;

using Engine = std::mt19937_64;
using Point = std::array<Real, kVariables>;
using Box = std::array<Interval, kVariables>;

struct Parameters
{
    Real constant;
    Point weights;
    Real exponent;
};

struct Check
{
    const Graph &graph;
    std::span<const Interval> box;
    std::span<const Interval> nodes;
    int &reported;
};

std::vector<int> sampled_codes();
Parameters parameters_of(Operator op, Engine &engine);
Graph operator_graph(Operator op, const Parameters &parameters);
Box box_of(Engine &engine);
std::vector<Interval> enclosures(const Graph &graph, std::span<const Interval> box);
void expect_sampled(const Check &check, Engine &engine);
std::string text(Real value);

}
