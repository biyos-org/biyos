#pragma once

#include "core/index.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <span>
#include <string>
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
    std::vector<double> result;
    std::vector<double> spare;
};

std::size_t lanes_of(std::size_t count);
std::size_t lanes_of(const Tape &tape, std::size_t count);
std::size_t sized(std::size_t size, std::size_t lanes);
std::string in_lanes(std::size_t lanes);
void check_sweep(const Tape &tape,
                 std::span<const double> point,
                 const Workspace &workspace,
                 std::span<const double> values);
void evaluate(const Instruction &instruction, const Frame &frame) noexcept;
void keep(const Instruction &instruction, const Frame &frame) noexcept;
void forward(const Block &block, std::span<const double> point, Workspace &workspace) noexcept;
void linearize(const Block &block, std::span<const double> point, Workspace &workspace) noexcept;
void store(const Block &block, const Workspace &workspace, std::span<double> outputs) noexcept;
void forward(const Tape &tape,
             std::span<const double> point,
             Workspace &workspace,
             std::span<double> values);
void broadcast(std::span<const double> point, std::size_t lanes, std::span<double> points);
void seed_directions(std::span<const Index> variables,
                     std::size_t lanes,
                     std::span<double> directions);

}
