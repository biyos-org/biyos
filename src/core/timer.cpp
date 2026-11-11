#include "core/timer.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

using std::chrono::nanoseconds;

std::optional<nanoseconds> cpu_of(const Clock &clock) noexcept
{
    try
    {
        return clock.cpu_time();
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void describe(Event &event,
              std::string_view module,
              std::string_view name,
              std::string_view edge,
              nanoseconds wall,
              nanoseconds cpu)
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
        .open = false,
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
    nodes_[node].open = true;
    current_ = node;
    return node;
}

std::size_t TimingTree::open_ancestor(std::size_t node) const noexcept
{
    std::size_t at = nodes_[node].parent;
    while (at != kNone && !nodes_[at].open)
    {
        at = nodes_[at].parent;
    }
    return at;
}

void TimingTree::leave(std::size_t node, nanoseconds wall, nanoseconds cpu) noexcept
{
    Node &timed = nodes_[node];
    timed.wall += wall;
    timed.cpu += cpu;
    ++timed.count;
    timed.open = false;
    if (node == current_)
    {
        current_ = open_ancestor(node);
    }
}

Timer::Timer(Environment &env, TimingTree &tree, FixedName module, FixedName name)
    : clock_(env.clock()), events_(env.events()), tree_(tree), module_(module.text()),
      wall_(clock_.wall_time()), cpu_(clock_.cpu_time()), observed_wall_(tree.observed_wall_),
      observed_cpu_(tree.observed_cpu_), node_(tree.enter(name.text()))
{
    announce("begin", wall_, cpu_);
}

Timer::~Timer()
{
    const nanoseconds wall = clock_.wall_time();
    const std::optional<nanoseconds> cpu = cpu_of(clock_);
    const nanoseconds observed_wall = tree_.observed_wall_ - observed_wall_;
    const nanoseconds observed_cpu = tree_.observed_cpu_ - observed_cpu_;
    const nanoseconds cpu_spent = cpu ? *cpu - cpu_ - observed_cpu : nanoseconds::zero();
    tree_.leave(node_, wall - wall_ - observed_wall, cpu_spent);
    announce("end", wall, cpu.value_or(cpu_));
}

void Timer::announce(std::string_view edge, nanoseconds wall, nanoseconds cpu) noexcept
{
    if (!events_.wanted(EventKind::Phase, Level::Info))
    {
        return;
    }
    const std::string_view name = tree_.nodes()[node_].name;
    const nanoseconds since = wall - events_.origin();
    const auto fill = [&](Event &event) { describe(event, module_, name, edge, since, cpu); };
    events_.emit(EventKind::Phase, Level::Info, fill);
    tree_.observed_wall_ += clock_.wall_time() - wall;
    tree_.observed_cpu_ += cpu_of(clock_).value_or(cpu) - cpu;
}

}
