#pragma once

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "io/nl/writer/defined.hpp"
#include "model/model.hpp"

#include <ostream>
#include <span>
#include <vector>

namespace biyos::nl
{

class Printer
{
  public:
    Printer(const Model &model,
            const Defined &defined,
            std::span<const Index> position,
            std::ostream &output);

    void print_top(Index function);
    void print_definition(const Definition &definition);

  private:
    enum class Piece
    {
        Opcode,
        Count,
        Number,
        Variable,
        Node,
    };

    struct Item
    {
        Piece piece;
        Offset node;
        Real number;
    };

    void run();
    void emit(const Item &item);
    void add(Piece piece, Offset node);
    void add_number(Real number);
    void add_scaled(Offset node, Real weight);
    void add_products(std::span<const QuadraticTerm> terms);
    void add_terms(Offset sum);
    void open(std::size_t count);
    void open_list(int code, std::size_t count);
    void expand(Offset node);
    void expand_sum(Offset node);
    void expand_product(Offset node);
    void expand_pieces(Offset node);

    const Model &model_;
    const Graph &graph_;
    const Defined &defined_;
    std::span<const Index> position_;
    std::ostream &output_;
    std::vector<Item> pieces_;
    std::vector<Item> stack_;
};

}
