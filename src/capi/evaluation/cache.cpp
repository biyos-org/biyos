#include "capi/evaluation/cache.hpp"

#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "model/evaluation/evaluation.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(biyos::Session &session)
{
    claim(session);
    return session.evaluation();
}

Running::Running(biyos::Session &session) noexcept : session_(session)
{
    session_.set_evaluating(true);
}

Running::~Running()
{
    session_.set_evaluating(false);
}

}
