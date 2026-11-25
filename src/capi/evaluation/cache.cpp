#include "capi/evaluation/cache.hpp"

#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "model/evaluation/evaluation.hpp"

#include <memory>

namespace biyos::capi
{

Evaluation &evaluation_of(BIYOS_model &model)
{
    claim(model);
    if (model.evaluation == nullptr)
    {
        const Owner owner{.model = &model, .hooks = &model.hooks};
        model.evaluation = std::make_unique<Evaluation>(model.handle.env(), model.core, owner);
    }
    return *model.evaluation;
}

Running::Running(BIYOS_model &model) noexcept : model_(model)
{
    model_.evaluating = true;
}

Running::~Running()
{
    model_.evaluating = false;
}

}
