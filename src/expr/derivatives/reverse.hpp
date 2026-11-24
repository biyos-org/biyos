#pragma once

#include "expr/derivatives/value.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Adjoints
{
    explicit Adjoints(const Tape &tape, std::size_t count = 1);

    std::size_t lanes;
    std::vector<double> slots;
    std::vector<double> shared;
    std::vector<double> gradient;
    std::vector<double> taken;
};

void drain(std::span<double> from, std::span<double> into) noexcept;

void retrace(const Block &block,
             std::size_t output,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<double> gradient) noexcept;
void reverse(const Block &block,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<double> gradient) noexcept;

}
