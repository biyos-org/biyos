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

inline constexpr std::size_t kRecords = 256;

struct Record
{
    BIYOS_status status = BIYOS_OK;
    const BIYOS_model *model = nullptr;
    std::string text;
};

struct RecordView
{
    BIYOS_status status;
    const BIYOS_model *model;
    const char *text;
};

class Records
{
  public:
    void put(BIYOS_status status, const BIYOS_model *model, std::string_view text) noexcept;
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
