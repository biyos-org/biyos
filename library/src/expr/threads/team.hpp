#pragma once

#include "core/numbers.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/tape/tape.hpp"
#include "expr/threads/levels.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

struct Worker
{
    Worker(const Tape &tape, std::size_t lanes);

    Workspace workspace;
    Adjoints adjoints;
};

struct Targets
{
    std::vector<std::size_t> variables;
    std::vector<Offset> starts;
    std::vector<Offset> rows;
    std::vector<Real> parts;
};

struct Team
{
    explicit Team(const Tape &tape, std::size_t width = 1);

    void fit(const Tape &tape, std::size_t threads);
    std::size_t size() const noexcept;
    Worker &lead() noexcept;

    std::size_t lanes;
    std::vector<Worker> members;
    Levels levels;
    Targets targets;
    std::vector<Real> pinned;
};

std::span<const std::size_t> variables_of(const Targets &targets, std::size_t block) noexcept;
std::span<Real> parts_of(Targets &targets, std::size_t block, std::size_t lanes) noexcept;

}
