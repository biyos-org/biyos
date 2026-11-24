#pragma once

#include "expr/tape/tape.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Workspace
{
    explicit Workspace(const Tape &tape, std::size_t count = 1);

    std::size_t lanes;
    std::vector<double> shared;
    std::vector<double> slots;
    std::vector<double> partials;
};

std::size_t lanes_of(std::size_t count);
void check_sweep(const Tape &tape,
                 std::span<const double> point,
                 const Workspace &workspace,
                 std::span<const double> values);
void forward(const Block &block, std::span<const double> point, Workspace &workspace) noexcept;
void linearize(const Block &block, std::span<const double> point, Workspace &workspace) noexcept;
void store(const Block &block, std::span<const double> slots, std::span<double> outputs) noexcept;
void forward(const Tape &tape,
             std::span<const double> point,
             Workspace &workspace,
             std::span<double> values);

}
