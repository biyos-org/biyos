#pragma once

namespace biyos
{

constexpr int kRoundedSteps = 1;
constexpr int kLibmUlps = 14;
constexpr int kLibmSteps = 256;

double shifted(double value, int steps) noexcept;
double round_down(double value) noexcept;
double round_up(double value) noexcept;
double widen_down(double value) noexcept;
double widen_up(double value) noexcept;

}
