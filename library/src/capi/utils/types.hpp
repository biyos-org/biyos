#pragma once

#include "biyos/biyos.h"
#include "core/numbers.hpp"
#include "core/options/options.hpp"

#include <concepts>

static_assert(std::same_as<biyos::Index, BIYOS_index>);
static_assert(std::same_as<biyos::Offset, BIYOS_offset>);
static_assert(std::same_as<biyos::Real, BIYOS_real>);
static_assert(std::same_as<biyos::Mask, BIYOS_mask>);
