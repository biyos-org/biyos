#pragma once

#include "expr/tape.hpp"

#include <span>
#include <vector>

namespace biyos
{

struct Workspace
{
    explicit Workspace(const Tape &tape);

    std::vector<double> shared;
    std::vector<double> slots;
    std::vector<double> partials;
};

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
