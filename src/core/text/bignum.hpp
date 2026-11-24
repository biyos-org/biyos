#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>

namespace biyos::text
{

class Bignum
{
  public:
    static constexpr std::size_t kLimbs = 64;
    static constexpr std::size_t kBits = kLimbs * 64;

    Bignum() = default;
    explicit Bignum(std::uint64_t value) noexcept;

    void add(std::uint64_t value) noexcept;
    void multiply(std::uint64_t factor) noexcept;
    void multiply_ten(std::size_t power) noexcept;
    void shift(std::size_t bits) noexcept;
    std::uint64_t divide(const Bignum &divisor) noexcept;

    std::size_t bits() const noexcept;
    bool is_zero() const noexcept;
    std::uint64_t limb(std::size_t index) const noexcept;

    friend std::strong_ordering operator<=>(const Bignum &a, const Bignum &b) noexcept;
    friend bool operator==(const Bignum &a, const Bignum &b) noexcept;

  private:
    void subtract(const Bignum &other) noexcept;
    std::uint64_t spliced(std::size_t index, std::size_t rest) const noexcept;
    void trim() noexcept;

    std::array<std::uint64_t, kLimbs> limbs_{};
    std::size_t size_ = 0;
};

}
