#pragma once

#include "expr/graph.hpp"

namespace biyos
{

class Rollback
{
  public:
    explicit Rollback(Graph &graph) noexcept : graph_(graph), mark_(graph.mark()) {}
    Rollback(const Rollback &) = delete;
    Rollback &operator=(const Rollback &) = delete;
    ~Rollback()
    {
        if (!kept_)
        {
            graph_.truncate(mark_);
        }
    }

    void keep() noexcept { kept_ = true; }

  private:
    Graph &graph_;
    Graph::Mark mark_;
    bool kept_ = false;
};

}
