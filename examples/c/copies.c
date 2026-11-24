#include <biyos/biyos.h>
#include <stdio.h>

static int fill(BIYOS_model *mdl)
{
    const BIYOS_real ub[] = {1.0, 2.0};
    const BIYOS_index fun[] = {0};
    if (BIYOS_addvar(mdl, 2, NULL, NULL, ub, NULL) != BIYOS_OK ||
        BIYOS_addfun(mdl, 1, NULL) != BIYOS_OK)
    {
        return 0;
    }
    return BIYOS_addcon(mdl, 1, fun, NULL, NULL, NULL) == BIYOS_OK;
}

static int count_vars(BIYOS_model *mdl)
{
    BIYOS_index count = -1;
    BIYOS_getnvar(mdl, &count);
    return count;
}

static int same_bounds(BIYOS_model *a, BIYOS_model *b)
{
    BIYOS_real ub_a[2];
    BIYOS_real ub_b[2];
    if (BIYOS_getvarrg(a, 0, 2, NULL, NULL, ub_a) != BIYOS_OK ||
        BIYOS_getvarrg(b, 0, 2, NULL, NULL, ub_b) != BIYOS_OK)
    {
        return 0;
    }
    return !(ub_a[0] < ub_b[0] || ub_a[0] > ub_b[0] || ub_a[1] < ub_b[1] ||
             ub_a[1] > ub_b[1]);
}

static int copy_and_change(BIYOS_model *mdl, BIYOS_env *other)
{
    BIYOS_model *copy = NULL;
    if (BIYOS_copymdl(mdl, other, &copy) != BIYOS_OK)
    {
        return 0;
    }
    int same = same_bounds(mdl, copy);
    int grown = BIYOS_addvar(copy, 1, NULL, NULL, NULL, NULL) == BIYOS_OK;
    printf("model %d variables, copy %d variables\n",
           count_vars(mdl),
           count_vars(copy));
    int apart = count_vars(mdl) == 2 && count_vars(copy) == 3;
    BIYOS_freemdl(copy);
    return same && grown && apart;
}

int main(void)
{
    BIYOS_env *env = NULL;
    BIYOS_env *other = NULL;
    BIYOS_model *mdl = NULL;
    int done = BIYOS_newenv(&env) == BIYOS_OK &&
               BIYOS_newenv(&other) == BIYOS_OK &&
               BIYOS_newmdl(env, &mdl) == BIYOS_OK && fill(mdl) &&
               copy_and_change(mdl, other);
    BIYOS_freemdl(mdl);
    BIYOS_freeenv(other);
    BIYOS_freeenv(env);
    return done ? 0 : 1;
}
