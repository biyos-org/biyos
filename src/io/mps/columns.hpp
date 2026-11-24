#pragma once

#include "core/index.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/records.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace biyos
{

class Columns
{
  public:
    void read(const Lines &lines, Draft &draft);
    void close(const Lines &lines) const;

  private:
    void mark(const Lines &lines, std::string_view keyword);
    Index enter(const Lines &lines, Draft &draft, std::string_view name);
    void add(const Lines &lines, Draft &draft, Index column, Fields pair);

    std::optional<Index> current_;
    bool marked_ = false;
    std::vector<Index> last_;
};

}
