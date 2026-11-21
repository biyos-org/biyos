#include <biyos/biyos.h>
#include <stdio.h>

static BIYOS_status variables(BIYOS_model *mdl)
{
    const BIYOS_int knd[] = {BIYOS_VAR_CONT, BIYOS_VAR_INT, BIYOS_VAR_BIN};
    const BIYOS_real lb[] = {0.0, 0.0, 0.0};
    const BIYOS_real ub[] = {4.0, 3.0, 1.0};
    return BIYOS_addvars(mdl, 3, knd, lb, ub, NULL);
}

static BIYOS_status functions(BIYOS_model *mdl)
{
    const BIYOS_real cst[] = {0.0, 0.0, 1.0};
    const BIYOS_offset off[] = {0, 3, 5, 6};
    const BIYOS_index varidx[] = {0, 1, 2, 0, 1, 0};
    const BIYOS_real coef[] = {1.0, 1.0, 1.0, 1.0, -2.0, -2.0};
    return BIYOS_addfuns(mdl, 3, cst, off, varidx, coef, NULL);
}

static BIYOS_status squares(BIYOS_model *mdl)
{
    const BIYOS_index funidx[] = {2};
    const BIYOS_offset off[] = {0, 2};
    const BIYOS_index varidx1[] = {0, 1};
    const BIYOS_index varidx2[] = {0, 1};
    const BIYOS_real coef[] = {1.0, 1.0};
    return BIYOS_addquadterms(mdl, 1, funidx, off, varidx1, varidx2, coef);
}

static BIYOS_status constraints(BIYOS_model *mdl)
{
    const BIYOS_index funidx[] = {0, 1};
    const BIYOS_real lb[] = {1.0, 0.0};
    const BIYOS_real ub[] = {5.0, 0.0};
    return BIYOS_addcons(mdl, 2, funidx, lb, ub, NULL);
}

static BIYOS_status objectives(BIYOS_model *mdl)
{
    const BIYOS_index funidx[] = {2, 0};
    const BIYOS_int sense[] = {BIYOS_MINIMIZE, BIYOS_MAXIMIZE};
    const BIYOS_int prio[] = {0, 1};
    const BIYOS_real tol[] = {0.5, 0.0};
    BIYOS_status status =
        BIYOS_addobjs(mdl, 2, funidx, sense, prio, NULL, tol, NULL, NULL);
    return status;
}

static int build(BIYOS_model *mdl)
{
    return variables(mdl) == BIYOS_OK && functions(mdl) == BIYOS_OK &&
           squares(mdl) == BIYOS_OK && constraints(mdl) == BIYOS_OK &&
           objectives(mdl) == BIYOS_OK;
}

static int read_squares(BIYOS_model *mdl)
{
    BIYOS_offset off[2];
    BIYOS_offset nnz = 0;
    BIYOS_index varidx1[2];
    BIYOS_index varidx2[2];
    BIYOS_real coef[2];
    BIYOS_status status =
        BIYOS_getquadterms(mdl, 2, 1, 2, off, &nnz, varidx1, varidx2, coef);
    if (status != BIYOS_OK)
    {
        return 0;
    }
    printf("function 2 has %lld quadratic terms\n", (long long)nnz);
    return nnz == 2 && varidx1[1] == 1 && varidx2[1] == 1;
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
        fprintf(stderr, "%s\n", BIYOS_lasterror(env));
    }
    BIYOS_freemdl(mdl);
    BIYOS_freeenv(env);
    return built ? 0 : 1;
}
