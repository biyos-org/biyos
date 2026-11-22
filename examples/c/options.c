#include <biyos/biyos.h>
#include <stdio.h>
#include <string.h>

static int set_options(BIYOS_env *env)
{
    BIYOS_int option = 0;
    return BIYOS_setenvdblopt(env, BIYOS_OPT_MAXTIME, 60.0) == BIYOS_OK &&
           BIYOS_setenvboolopt(env, BIYOS_OPT_PRESOLVE, 0) == BIYOS_OK &&
           BIYOS_getoptid(env, "check_lvl", &option) == BIYOS_OK &&
           BIYOS_setenvstropt(env, option, "full") == BIYOS_OK;
}

static int read_options(BIYOS_env *env)
{
    BIYOS_real limit = 0.0;
    BIYOS_int presolve = 1;
    const char *lvl = NULL;
    if (BIYOS_getenvdblopt(env, BIYOS_OPT_MAXTIME, &limit) != BIYOS_OK ||
        BIYOS_getenvboolopt(env, BIYOS_OPT_PRESOLVE, &presolve) !=
            BIYOS_OK ||
        BIYOS_getenvstropt(env, BIYOS_OPT_CHECKLVL, &lvl) != BIYOS_OK)
    {
        return 0;
    }
    printf("maxtime %g, presolve %d, check_lvl %s\n", limit, presolve, lvl);
    return presolve == 0 && strcmp(lvl, "full") == 0;
}

static int describe(BIYOS_env *env, const char *name)
{
    BIYOS_int opt = 0;
    BIYOS_int knd = 0;
    if (BIYOS_getoptid(env, name, &opt) != BIYOS_OK ||
        BIYOS_getoptknd(env, opt, &knd) != BIYOS_OK)
    {
        return 0;
    }
    BIYOS_int since = 0;
    BIYOS_int deprecated = 0;
    BIYOS_int retired = 0;
    BIYOS_status status =
        BIYOS_getoptver(env, opt, &since, &deprecated, &retired);
    if (status != BIYOS_OK)
    {
        return 0;
    }
    printf("%s: id %d, kind %d, since %d\n", name, opt, knd, since);
    return knd == BIYOS_KND_ENUM && deprecated == 0 && retired == 0;
}

int main(void)
{
    BIYOS_env *env = NULL;
    if (BIYOS_newenv(&env) != BIYOS_OK)
    {
        return 1;
    }
    int done =
        set_options(env) && read_options(env) && describe(env, "check_lvl");
    if (!done)
    {
        fprintf(stderr, "%s\n", BIYOS_lasterror(env));
    }
    BIYOS_freeenv(env);
    return done ? 0 : 1;
}
