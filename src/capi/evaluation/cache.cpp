#include "capi/evaluation/cache.hpp"

#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "model/evaluation/evaluation.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(BIYOS_model &session)
{
    claim(session);
    return session.evaluation();
}

Running::Running(BIYOS_model &session) noexcept : session_(session)
{
    session_.set_evaluating(true);
}

Running::~Running()
{
    session_.set_evaluating(false);
}

}
