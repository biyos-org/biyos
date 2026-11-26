#pragma once

#include "core/numbers.hpp"
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
    std::vector<Real> slots;
    std::vector<Real> shared;
    std::vector<Real> gradient;
    std::vector<Real> taken;
};

void drain(std::span<Real> from, std::span<Real> into) noexcept;

void retrace(const Block &block,
             std::size_t output,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<Real> gradient) noexcept;
void reverse(const Block &block,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<Real> gradient) noexcept;

}
