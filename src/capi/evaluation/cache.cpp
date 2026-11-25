#include "capi/evaluation/cache.hpp"

#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "model/evaluation/evaluation.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(BIYOS_model &model)
{
    claim(model);
    return model.evaluation();
}

Running::Running(BIYOS_model &model) noexcept : model_(model)
{
    model_.set_evaluating(true);
}

Running::~Running()
{
    model_.set_evaluating(false);
}

}
