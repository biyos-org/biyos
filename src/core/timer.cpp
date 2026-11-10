#include "core/timer.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

using std::chrono::nanoseconds;

nanoseconds cpu_or(const Clock &clock, nanoseconds fallback) noexcept
{
    try
    {
        return clock.cpu_time();
    }
    catch (...)
    {
        return fallback;
    }
}

void describe(Event &event, std::string_view module, std::string_view name, std::string_view edge,
              nanoseconds wall, nanoseconds cpu)
{
    event.module.assign(module);
    event.text.assign(name);
    event.fields.push_back(Field{.name = "edge", .value = std::string(edge)});
    event.fields.push_back(Field{.name = "wall", .value = std::to_string(wall.count())});
    event.fields.push_back(Field{.name = "cpu", .value = std::to_string(cpu.count())});
}

}

std::size_t TimingTree::find(std::string_view name) const noexcept
{
    for (std::size_t node = 0; node < nodes_.size(); ++node)
    {
        if (nodes_[node].parent == current_ && nodes_[node].name == name)
        {
            return node;
        }
    }
    return kNone;
}

std::size_t TimingTree::add(std::string_view name)
{
    nodes_.push_back(Node{
        .name = name,
        .parent = current_,
        .wall = nanoseconds::zero(),
        .cpu = nanoseconds::zero(),
        .count = 0,
    });
    return nodes_.size() - 1;
}

std::size_t TimingTree::enter(std::string_view name)
{
    std::size_t node = find(name);
    if (node == kNone)
    {
        node = add(name);
    }
    current_ = node;
    return node;
}

void TimingTree::leave(std::size_t node, nanoseconds wall, nanoseconds cpu) noexcept
{
    Node &timed = nodes_[node];
    timed.wall += wall;
    timed.cpu += cpu;
    ++timed.count;
    current_ = timed.parent;
}

Timer::Timer(Environment &env, TimingTree &tree, FixedName module, FixedName name)
    : clock_(env.clock()), events_(env.events()), tree_(tree), module_(module.text()),
      wall_(clock_.wall_time()), cpu_(clock_.cpu_time()), node_(tree.enter(name.text()))
{
    announce("begin", wall_, cpu_);
}

Timer::~Timer()
{
    const nanoseconds wall = clock_.wall_time();
    const nanoseconds cpu = cpu_or(clock_, cpu_);
    tree_.leave(node_, wall - wall_, cpu - cpu_);
    announce("end", wall, cpu);
}

void Timer::announce(std::string_view edge, nanoseconds wall, nanoseconds cpu) noexcept
{
    const std::string_view name = tree_.nodes()[node_].name;
    const nanoseconds since = wall - events_.origin();
    const auto fill = [&](Event &event) { describe(event, module_, name, edge, since, cpu); };
    events_.emit(EventKind::Phase, Level::Info, fill);
}

}
