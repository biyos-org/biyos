#pragma once

#include "biyos/biyos.h"
#include "core/thread.hpp"

#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <string_view>

namespace biyos
{

class Session;

inline constexpr std::size_t kRecords = 256;

struct Record
{
    BIYOS_status status = BIYOS_OK;
    const Session *model = nullptr;
    std::string text;
};

struct RecordView
{
    BIYOS_status status;
    const Session *model;
    const char *text;
};

class Records
{
  public:
    void put(BIYOS_status status, const Session *model, std::string_view text) noexcept;
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
