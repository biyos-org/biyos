#include <biyos/biyos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_text(const BIYOS_event *event, void *data)
{
    char line[256];
    size_t len = 0;
    if (BIYOS_fmttext(event, sizeof line, line, &len) == BIYOS_OK)
    {
        printf("%s%s\n", line, len < sizeof line ? "" : "...");
    }
    int *count = data;
    *count += 1;
}

static void print_json(const BIYOS_event *event, void *data)
{
    size_t len = 0;
    if (BIYOS_fmtjson(event, 0, NULL, &len) != BIYOS_OK)
    {
        return;
    }
    char *line = malloc(len + 1);
    if (line != NULL &&
        BIYOS_fmtjson(event, len + 1, line, &len) == BIYOS_OK)
    {
        printf("%s\n", line);
    }
    free(line);
    (void)data;
}

static int code_of(const BIYOS_event *event)
{
    for (BIYOS_index field = 0; field < BIYOS_getnevtfld(event); ++field)
    {
        if (strcmp(BIYOS_getevtfldname(event, field), "code") == 0)
        {
            return atoi(BIYOS_getevtfldval(event, field));
        }
    }
    return 0;
}

static void keep_code(const BIYOS_event *event, void *data)
{
    int *code = data;
    *code = code_of(event);
}

static int hook(BIYOS_env *env, BIYOS_hook fn, void *data, int lvl)
{
    const BIYOS_int knd = BIYOS_EVT_MESSAGE;
    BIYOS_index idx = 0;
    return BIYOS_addenvhook(env, fn, data, knd, lvl, &idx) == BIYOS_OK;
}

static int hook_model(BIYOS_model *mdl, BIYOS_hook fn, void *data, int lvl)
{
    const BIYOS_int knd = BIYOS_EVT_MESSAGE;
    BIYOS_index idx = 0;
    return BIYOS_addmdlhook(mdl, fn, data, knd, lvl, &idx) == BIYOS_OK;
}

int main(void)
{
    BIYOS_env *env = NULL;
    if (BIYOS_newenv(&env) != BIYOS_OK)
    {
        return 1;
    }
    BIYOS_model *mdl = NULL;
    int count = 0;
    int code = 0;
    int hooked = hook(env, print_text, &count, BIYOS_LVL_INFO) &&
                 hook(env, print_json, NULL, BIYOS_LVL_ERR) &&
                 BIYOS_newmdl(env, &mdl) == BIYOS_OK &&
                 hook_model(mdl, keep_code, &code, BIYOS_LVL_ERR);
    BIYOS_status status = BIYOS_setmdlintopt(mdl, BIYOS_OPT_SEED, -1);
    BIYOS_freemdl(mdl);
    BIYOS_freeenv(env);
    int seen = count == 1 && code == BIYOS_ERR_BADARG;
    return hooked && status == BIYOS_ERR_BADARG && seen ? 0 : 1;
}
