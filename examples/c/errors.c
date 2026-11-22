#include <biyos/biyos.h>
#include <stdio.h>

static const char *at_fault(BIYOS_status status)
{
    if (status <= -500)
    {
        return "biyos";
    }
    if (status <= -400)
    {
        return "resources";
    }
    if (status <= -300)
    {
        return "user code";
    }
    if (status <= -200)
    {
        return "the model";
    }
    return "the caller";
}

static int report(BIYOS_env *env, BIYOS_status status)
{
    if (status == BIYOS_OK)
    {
        return 0;
    }
    printf("%d, at fault %s: %s\n",
           status,
           at_fault(status),
           BIYOS_lasterror(env));
    return 1;
}

int main(void)
{
    BIYOS_env *env = NULL;
    BIYOS_model *mdl = NULL;
    if (BIYOS_newenv(&env) != BIYOS_OK ||
        BIYOS_newmdl(env, &mdl) != BIYOS_OK)
    {
        return 1;
    }
    const BIYOS_int knd[] = {BIYOS_VAR_BIN};
    const BIYOS_real ub[] = {2.0};
    int failed = report(env, BIYOS_addvar(mdl, 1, knd, NULL, ub, NULL));
    BIYOS_int seed = -1;
    failed += report(env, BIYOS_setenvintopt(env, BIYOS_OPT_SEED, -1));
    int kept = BIYOS_getenvintopt(env, BIYOS_OPT_SEED, &seed) == BIYOS_OK &&
               seed == 0;
    BIYOS_freemdl(mdl);
    BIYOS_freeenv(env);
    return failed == 2 && kept ? 0 : 1;
}
