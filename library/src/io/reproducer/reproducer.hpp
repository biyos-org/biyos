#pragma once

#include "core/env/env.hpp"
#include "core/options/lookup.hpp"
#include "core/options/settings.hpp"
#include "model/model.hpp"

#include <istream>
#include <ostream>
#include <string>

namespace biyos
{

void print_reproducer(const Model &model, const Lookup &lookup, std::ostream &output);
Model read_reproducer(Env &env, Settings &settings, std::istream &input, const std::string &file);

}
