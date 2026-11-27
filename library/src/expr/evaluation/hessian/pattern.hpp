#pragma once

#include "core/numbers.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

struct Shape
{
    std::vector<Entry> entries;
    Offset pushes;
};

class Pusher
{
  public:
    explicit Pusher(const Tape &tape);

    Shape shape(Offset element);

  private:
    enum class Form
    {
        Constant,
        Variable,
        Unary,
        Binary,
        Listed,
    };

    enum class Bending
    {
        None,
        Full,
        Quotient,
        Cross,
    };

    struct Rule
    {
        Form form;
        Bending bending;
    };

    struct Node
    {
        Bending bending;
        std::uint32_t begin;
        std::uint32_t end;
    };

    static Rule rule_of(const Instruction &instruction) noexcept;
    void start(Offset element);
    void walk(const Block &block, std::size_t output);
    void rise(Offset element);
    void enter(const Instruction &instruction, std::span<const std::uint32_t> operands);
    void gather(const Instruction &instruction, Form form, std::span<const std::uint32_t> operands);
    void eliminate();
    void push(std::uint32_t id, std::span<const std::uint32_t> operands);
    void create(const Node &node, std::span<const std::uint32_t> operands);
    void pairs(std::span<const std::uint32_t> operands);
    void link(std::uint32_t a, std::uint32_t b);
    std::vector<Entry> entries();

    const Tape &tape_;
    std::vector<std::uint32_t> local_;
    std::vector<std::uint32_t> occupants_;
    std::vector<std::uint32_t> held_;
    std::vector<Node> nodes_;
    std::vector<std::uint32_t> operands_;
    std::vector<std::vector<std::uint32_t>> rows_;
    std::uint32_t variables_ = 0;
    Offset pushes_ = 0;
};

}
