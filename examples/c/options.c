#include <biyos/biyos.h>
#include <stdio.h>

static int set_options(BIYOS_env *env)
{
    BIYOS_int option = 0;
    return BIYOS_setenvdblopt(env, BIYOS_OPT_MAXTIME, 60.0) == BIYOS_OK &&
           BIYOS_setenvboolopt(env, BIYOS_OPT_PRESOLVE, 0) == BIYOS_OK &&
           BIYOS_getoptid(env, "checking", &option) == BIYOS_OK &&
           BIYOS_setenvintopt(env, option, BIYOS_CHK_FULL) == BIYOS_OK;
}

static int read_options(BIYOS_env *env)
{
    BIYOS_real limit = 0.0;
    BIYOS_int presolve = 1;
    BIYOS_int lvl = 0;
    if (BIYOS_getenvdblopt(env, BIYOS_OPT_MAXTIME, &limit) != BIYOS_OK ||
        BIYOS_getenvboolopt(env, BIYOS_OPT_PRESOLVE, &presolve) !=
            BIYOS_OK ||
        BIYOS_getenvintopt(env, BIYOS_OPT_CHK, &lvl) != BIYOS_OK)
    {
        return 0;
    }
    printf("maxtime %g, presolve %d, checking %d\n", limit, presolve, lvl);
    return presolve == 0 && lvl == BIYOS_CHK_FULL;
}

static int model_options(BIYOS_env *env)
{
    BIYOS_model *mdl = NULL;
    if (BIYOS_newmdl(env, &mdl) != BIYOS_OK)
    {
        return 0;
    }
    BIYOS_real own = 0.0;
    BIYOS_real shared = 0.0;
    BIYOS_int src = 0;
    int done =
        BIYOS_setmdldblopt(mdl, BIYOS_OPT_MAXTIME, 5.0) == BIYOS_OK &&
        BIYOS_getmdldblopt(mdl, BIYOS_OPT_MAXTIME, &own) == BIYOS_OK &&
        BIYOS_delmdlopt(mdl, BIYOS_OPT_MAXTIME) == BIYOS_OK &&
        BIYOS_getmdldblopt(mdl, BIYOS_OPT_MAXTIME, &shared) == BIYOS_OK &&
        BIYOS_getmdloptsrc(mdl, BIYOS_OPT_MAXTIME, &src) == BIYOS_OK;
    if (done)
    {
        printf("own %g, shared %g, source %d\n", own, shared, src);
    }
    BIYOS_freemdl(mdl);
    return done && src == BIYOS_OPTSRC_ENV;
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
    return knd == BIYOS_OPTKND_ENUM && deprecated == 0 && retired == 0;
}

int main(void)
{
    BIYOS_env *env = NULL;
    if (BIYOS_newenv(&env) != BIYOS_OK)
    {
        return 1;
    }
    int done = set_options(env) && read_options(env) &&
               model_options(env) && describe(env, "checking");
    if (!done)
    {
        BIYOS_error error = {.size = sizeof error};
        BIYOS_lasterror(env, &error);
        fprintf(stderr, "%s\n", error.msg);
    }
    BIYOS_freeenv(env);
    return done ? 0 : 1;
}
