#pragma once

#include "core/index.hpp"
#include "core/text/text.hpp"

#include <initializer_list>
#include <string>
#include <string_view>

namespace biyos
{

template <typename T> std::string against(std::string_view field, T a, T b)
{
    return std::string(field) + " " + text_of(a) + " against " + text_of(b);
}

std::string compared(std::string_view field, Index a, Index b);
std::string compared(std::string_view field, Offset a, Offset b);
std::string compared(std::string_view field, double a, double b);
std::string compared(std::string_view field, const std::string *a, const std::string *b);
std::string first_of(std::initializer_list<std::string> found);

}
