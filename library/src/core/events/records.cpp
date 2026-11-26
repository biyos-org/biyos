#include "core/events/records.hpp"

#include <algorithm>
#include <mutex>
#include <utility>

namespace biyos
{

namespace
{

void assign(std::string &kept, std::string_view text) noexcept
{
    try
    {
        kept.assign(text);
    }
    catch (...)
    {
        kept.clear();
    }
}

}

void Records::put(ErrorKind kind, const Session *model, std::string_view text) noexcept
{
    try
    {
        const std::lock_guard held(lock_);
        Record &record = slot(this_thread());
        record.kind = kind;
        record.model = model;
        assign(record.text, text);
    }
    catch (...)
    {
    }
}

RecordView Records::get() const noexcept
{
    const std::lock_guard held(lock_);
    const auto found = std::ranges::find(entries_, this_thread(), &Entry::thread);
    const Record &record = found == entries_.end() ? none_ : found->record;
    return RecordView{.kind = record.kind, .model = record.model, .text = record.text.c_str()};
}

void Records::swap(Record &record) noexcept
{
    try
    {
        const std::lock_guard held(lock_);
        const auto found = std::ranges::find(entries_, this_thread(), &Entry::thread);
        if (found != entries_.end())
        {
            std::swap(found->record, record);
        }
        else if (record.kind.has_value())
        {
            std::swap(slot(this_thread()), record);
        }
    }
    catch (...)
    {
    }
}

Record &Records::slot(ThreadId thread)
{
    const auto found = std::ranges::find(entries_, thread, &Entry::thread);
    if (found != entries_.end())
    {
        entries_.splice(entries_.end(), entries_, found);
        return found->record;
    }
    entries_.push_back(Entry{.thread = thread, .record = {}});
    if (entries_.size() > kRecords)
    {
        entries_.pop_front();
    }
    return entries_.back().record;
}

}
