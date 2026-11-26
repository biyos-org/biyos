#pragma once

#include "core/error.hpp"
#include "core/thread.hpp"

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace biyos
{

class Session;

inline constexpr std::size_t kRecords = 256;

struct Record
{
    std::optional<ErrorKind> kind;
    const Session *model = nullptr;
    std::string text;
};

struct RecordView
{
    std::optional<ErrorKind> kind;
    const Session *model;
    const char *text;
};

class Records
{
  public:
    void put(ErrorKind kind, const Session *model, std::string_view text) noexcept;
    RecordView get() const noexcept;
    void swap(Record &record) noexcept;

  private:
    struct Entry
    {
        ThreadId thread;
        Record record;
    };

    Record &slot(ThreadId thread);

    mutable std::mutex lock_;
    std::list<Entry> entries_;
    Record none_;
};

}
