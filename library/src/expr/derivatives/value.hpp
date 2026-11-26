#pragma once

#include "core/numbers.hpp"
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
    std::vector<Real> shared;
    std::vector<Real> slots;
    std::vector<Real> partials;
    std::vector<Real> result;
    std::vector<Real> spare;
};

std::size_t lanes_of(std::size_t count);
std::size_t lanes_of(const Tape &tape, std::size_t count);
std::size_t sized(std::size_t size, std::size_t lanes);
std::string in_lanes(std::size_t lanes);
void check_sweep(const Tape &tape,
                 std::span<const Real> point,
                 const Workspace &workspace,
                 std::span<const Real> values);
void evaluate(const Instruction &instruction, const Frame &frame) noexcept;
void keep(const Instruction &instruction, const Frame &frame) noexcept;
void forward(const Block &block, std::span<const Real> point, Workspace &workspace) noexcept;
void forward(const Block &block,
             std::span<const Real> point,
             std::span<const Real> shared,
             Workspace &workspace) noexcept;
void linearize(const Block &block, std::span<const Real> point, Workspace &workspace) noexcept;
void linearize(const Block &block,
               std::span<const Real> point,
               std::span<const Real> shared,
               std::span<Real> partials,
               Workspace &workspace) noexcept;
void store(const Block &block, const Workspace &workspace, std::span<Real> outputs) noexcept;
void forward(const Tape &tape,
             std::span<const Real> point,
             Workspace &workspace,
             std::span<Real> values);
void broadcast(std::span<const Real> point, std::size_t lanes, std::span<Real> points);
void repeat(std::span<const Real> point, std::size_t lanes, std::span<Real> points) noexcept;
void seed_directions(std::span<const Index> variables,
                     std::size_t lanes,
                     std::span<Real> directions);

}
