#pragma once

#include "expr/tape.hpp"
#include "expr/value.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Adjoints
{
    explicit Adjoints(const Tape &tape);

    std::vector<double> slots;
    std::vector<double> shared;
    std::vector<double> gradient;
};

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
