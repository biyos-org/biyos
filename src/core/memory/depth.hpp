#pragma once

namespace biyos
{

class Depth
{
  public:
    explicit Depth(int &n) noexcept : count_(n) { ++count_; }

    ~Depth() { --count_; }

    Depth(const Depth &) = delete;
    Depth &operator=(const Depth &) = delete;

  private:
    int &count_;
};

}
