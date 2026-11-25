#include <biyos/biyos.h>
#include <stdio.h>

static BIYOS_status variables(BIYOS_model *mdl)
{
    const BIYOS_int knd[] = {BIYOS_VARKND_CONT,
                             BIYOS_VARKND_INT,
                             BIYOS_VARKND_BIN};
    const BIYOS_real lb[] = {0.0, 0.0, 0.0};
    const BIYOS_real ub[] = {4.0, 3.0, 1.0};
    return BIYOS_addvar(mdl, 3, knd, lb, ub, NULL);
}

static BIYOS_status functions(BIYOS_model *mdl)
{
    const BIYOS_real cst[] = {0.0, 0.0, 1.0};
    const BIYOS_offset cnt[] = {3, 2, 1};
    const BIYOS_index var[] = {0, 1, 2, 0, 1, 0};
    const BIYOS_real coef[] = {1.0, 1.0, 1.0, 1.0, -2.0, -2.0};
    BIYOS_index pos = 0;
    BIYOS_status status = BIYOS_addfun(mdl, 3, &pos);
    if (status != BIYOS_OK)
    {
        return status;
    }
    status = BIYOS_setcstrg(mdl, pos, pos + 3, cst);
    if (status != BIYOS_OK)
    {
        return status;
    }
    return BIYOS_addrlintermrg(mdl, pos, pos + 3, cnt, var, coef);
}

static BIYOS_status squares(BIYOS_model *mdl)
{
    const BIYOS_index fun[] = {2, 2};
    const BIYOS_index var1[] = {0, 1};
    const BIYOS_index var2[] = {0, 1};
    const BIYOS_real coef[] = {1.0, 1.0};
    return BIYOS_addquadtermsp(mdl, 2, fun, var1, var2, coef);
}

static BIYOS_status constraints(BIYOS_model *mdl)
{
    const BIYOS_index fun[] = {0, 1};
    const BIYOS_real lb[] = {1.0, 0.0};
    const BIYOS_real ub[] = {5.0, 0.0};
    return BIYOS_addcon(mdl, 2, fun, lb, ub, NULL);
}

static BIYOS_status objectives(BIYOS_model *mdl)
{
    const BIYOS_index fun[] = {2, 0};
    const BIYOS_int sense[] = {BIYOS_OBJSENSE_MIN, BIYOS_OBJSENSE_MAX};
    const BIYOS_int prio[] = {0, 1};
    const BIYOS_real atol[] = {0.5};
    BIYOS_index pos = 0;
    BIYOS_status status =
        BIYOS_addobj(mdl, 2, fun, sense, prio, NULL, &pos);
    if (status != BIYOS_OK)
    {
        return status;
    }
    return BIYOS_setobjtolsp(mdl, 1, &pos, atol, NULL);
}

static int build(BIYOS_model *mdl)
{
    return variables(mdl) == BIYOS_OK && functions(mdl) == BIYOS_OK &&
           squares(mdl) == BIYOS_OK && constraints(mdl) == BIYOS_OK &&
           objectives(mdl) == BIYOS_OK;
}

static int read_squares(BIYOS_model *mdl)
{
    BIYOS_offset cnt[1];
    BIYOS_offset nnz = 0;
    BIYOS_index var1[2];
    BIYOS_index var2[2];
    BIYOS_real coef[2];
    BIYOS_status status = BIYOS_getnquadtermrg(mdl, 2, 3, &nnz);
    if (status != BIYOS_OK || nnz > 2)
    {
        return 0;
    }
    status = BIYOS_getquadtermrg(mdl, 2, 3, nnz, cnt, var1, var2, coef);
    if (status != BIYOS_OK)
    {
        return 0;
    }
    printf("function 2 has %lld quadratic terms\n", (long long)nnz);
    return nnz == 2 && var1[1] == 1 && var2[1] == 1;
}

int main(void)
{
    BIYOS_env *env = NULL;
    BIYOS_model *mdl = NULL;
    if (BIYOS_newenv(&env) != BIYOS_OK)
    {
        return 1;
    }
    int built = BIYOS_newmdl(env, &mdl) == BIYOS_OK && build(mdl) &&
                read_squares(mdl);
    if (!built)
    {
        BIYOS_error error = {.size = sizeof error};
        BIYOS_lasterror(env, &error);
        fprintf(stderr, "%s\n", error.msg);
    }
    BIYOS_freemdl(mdl);
    BIYOS_freeenv(env);
    return built ? 0 : 1;
}
