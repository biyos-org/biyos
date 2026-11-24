#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/interval.hpp"
#include "expr/shape.hpp"
#include "sampling.hpp"

#include <ostream>
#include <span>
#include <string>

namespace biyos
{

void PrintTo(Curvature curvature, std::ostream *out);
void PrintTo(Monotonicity monotonicity, std::ostream *out);

}

namespace biyos::test
{

struct Claims
{
    const Graph &graph;
    std::span<const Interval> box;
    std::span<const Curvature> curvatures;
    int &reported;
};

struct Monotonicities
{
    const Graph &graph;
    std::span<const Interval> box;
    std::span<const Monotonicity> monotonicities;
    int &reported;
};

std::string name_of(Curvature curvature);
std::string name_of(Monotonicity monotonicity);
void expect_chords(const Claims &claims, Engine &engine);
void expect_monotonicities(const Monotonicities &monotonicities, Engine &engine);
void expect_root(const Graph &graph,
                 std::span<const Interval> box,
                 Curvature curvature,
                 std::span<const Monotonicity> monotonicities);

}
