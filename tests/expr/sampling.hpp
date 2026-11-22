#pragma once

#include "expr/graph.hpp"
#include "expr/interval.hpp"
#include "expr/node.hpp"

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
using Point = std::array<double, kVariables>;
using Box = std::array<Interval, kVariables>;

struct Parameters
{
    double constant;
    Point weights;
    double exponent;
};

struct Check
{
    const Graph &graph;
    std::span<const Interval> box;
    std::span<const Interval> nodes;
    int &reported;
};

Parameters parameters_of(Operator op, Engine &engine);
Graph operator_graph(Operator op, const Parameters &parameters);
Box box_of(Engine &engine);
std::vector<Interval> enclosures(const Graph &graph, std::span<const Interval> box);
void expect_sampled(const Check &check, Engine &engine);
std::string text(double value);

}
