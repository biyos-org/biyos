#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#if defined(BIYOS_STATIC)
#define BIYOS_API
#elif defined(_WIN32)
#if defined(BIYOS_BUILDING)
#define BIYOS_API __declspec(dllexport)
#else
#define BIYOS_API __declspec(dllimport)
#endif
#else
#define BIYOS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef int BIYOS_status;
typedef int32_t BIYOS_index;
typedef int32_t BIYOS_int;
typedef int64_t BIYOS_offset;
typedef double BIYOS_real;
typedef uint64_t BIYOS_mask;
typedef struct BIYOS_env BIYOS_env;
typedef struct BIYOS_event BIYOS_event;
typedef struct BIYOS_model BIYOS_model;
typedef void (*BIYOS_hook)(const BIYOS_event *evt, void *data);

typedef struct BIYOS_error
{
    size_t size;
    BIYOS_status status;
    const BIYOS_model *mdl;
    const char *msg;
} BIYOS_error;

#define BIYOS_INF INFINITY
#define BIYOS_DENSE (-1)

#define BIYOS_OK 0
#define BIYOS_ERR_BADARG (-100)
#define BIYOS_ERR_BADFILE (-200)
#define BIYOS_ERR_UNSUPPORTED (-201)
#define BIYOS_ERR_BUSY (-202)
#define BIYOS_ERR_INUSE (-203)
#define BIYOS_ERR_CALLBACK (-300)
#define BIYOS_ERR_NOMEMORY (-400)
#define BIYOS_ERR_FILEIO (-401)
#define BIYOS_ERR_INTERNAL (-500)

BIYOS_API void BIYOS_version(BIYOS_int *major, BIYOS_int *minor, BIYOS_int *patch);

BIYOS_API BIYOS_status BIYOS_newenv(BIYOS_env **env);
BIYOS_API BIYOS_status BIYOS_freeenv(BIYOS_env *env);
BIYOS_API BIYOS_status BIYOS_lasterror(const BIYOS_env *env, BIYOS_error *err);
BIYOS_API BIYOS_status BIYOS_interrupt(BIYOS_env *env);

#define BIYOS_OPT_SEED 1
#define BIYOS_OPT_MAXTIME 2
#define BIYOS_OPT_CHK 3
#define BIYOS_OPT_PRESOLVE 4
#define BIYOS_OPT_REPRO 5
#define BIYOS_OPT_FEASTOLABS 6
#define BIYOS_OPT_FEASTOLREL 7
#define BIYOS_OPT_INTTOL 8
#define BIYOS_OPT_ZEROTOL 9
#define BIYOS_OPT_MAXCPUTIME 10
#define BIYOS_OPT_THREADS 11

#define BIYOS_CHK_NONE 1
#define BIYOS_CHK_CHEAP 2
#define BIYOS_CHK_FULL 3

#define BIYOS_OPTKND_BOOL 1
#define BIYOS_OPTKND_INT 2
#define BIYOS_OPTKND_DBL 3
#define BIYOS_OPTKND_STR 4
#define BIYOS_OPTKND_ENUM 5
#define BIYOS_OPTKND_FLG 6

#define BIYOS_OPTSRC_MDL 1
#define BIYOS_OPTSRC_ENV 2
#define BIYOS_OPTSRC_DEFAULT 3

BIYOS_API BIYOS_status BIYOS_getoptid(BIYOS_env *env, const char *name, BIYOS_int *opt);
BIYOS_API BIYOS_status BIYOS_getoptknd(BIYOS_env *env, BIYOS_int opt, BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_getoptver(BIYOS_env *env,
                                       BIYOS_int opt,
                                       BIYOS_int *since,
                                       BIYOS_int *deprecated,
                                       BIYOS_int *retired);
BIYOS_API BIYOS_status BIYOS_setenvboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val);
BIYOS_API BIYOS_status BIYOS_setenvintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val);
BIYOS_API BIYOS_status BIYOS_setenvdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real val);
BIYOS_API BIYOS_status BIYOS_setenvstropt(BIYOS_env *env, BIYOS_int opt, const char *val);
BIYOS_API BIYOS_status BIYOS_setenvflgopt(BIYOS_env *env,
                                          BIYOS_int opt,
                                          BIYOS_mask on,
                                          BIYOS_mask off);
BIYOS_API BIYOS_status BIYOS_getenvboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getenvintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getenvdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real *val);
BIYOS_API BIYOS_status BIYOS_getenvstropt(BIYOS_env *env, BIYOS_int opt, const char **val);
BIYOS_API BIYOS_status BIYOS_getenvflgopt(BIYOS_env *env,
                                          BIYOS_int opt,
                                          BIYOS_mask *on,
                                          BIYOS_mask *off);
BIYOS_API BIYOS_status BIYOS_setmdlboolopt(BIYOS_model *mdl, BIYOS_int opt, BIYOS_int val);
BIYOS_API BIYOS_status BIYOS_setmdlintopt(BIYOS_model *mdl, BIYOS_int opt, BIYOS_int val);
BIYOS_API BIYOS_status BIYOS_setmdldblopt(BIYOS_model *mdl, BIYOS_int opt, BIYOS_real val);
BIYOS_API BIYOS_status BIYOS_setmdlstropt(BIYOS_model *mdl, BIYOS_int opt, const char *val);
BIYOS_API BIYOS_status BIYOS_setmdlflgopt(BIYOS_model *mdl,
                                          BIYOS_int opt,
                                          BIYOS_mask on,
                                          BIYOS_mask off);
BIYOS_API BIYOS_status BIYOS_getmdlboolopt(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getmdlintopt(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getmdldblopt(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_real *val);
BIYOS_API BIYOS_status BIYOS_getmdlstropt(const BIYOS_model *mdl, BIYOS_int opt, const char **val);
BIYOS_API BIYOS_status BIYOS_getmdlflgopt(const BIYOS_model *mdl,
                                          BIYOS_int opt,
                                          BIYOS_mask *on,
                                          BIYOS_mask *off);
BIYOS_API BIYOS_status BIYOS_delenvopt(BIYOS_env *env, BIYOS_int opt);
BIYOS_API BIYOS_status BIYOS_delmdlopt(BIYOS_model *mdl, BIYOS_int opt);
BIYOS_API BIYOS_status BIYOS_getmdloptsrc(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_int *src);

#define BIYOS_EVTKND_MSG 1
#define BIYOS_EVTKND_INV 2
#define BIYOS_EVTKND_SPAN 3

#define BIYOS_EVTLVL_DBG 1
#define BIYOS_EVTLVL_INFO 2
#define BIYOS_EVTLVL_WARN 3
#define BIYOS_EVTLVL_ERR 4

BIYOS_API BIYOS_status BIYOS_addenvhook(BIYOS_env *env,
                                        BIYOS_hook hk,
                                        void *data,
                                        BIYOS_int knd,
                                        BIYOS_int lvl,
                                        BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delenvhook(BIYOS_env *env, BIYOS_index idx);
BIYOS_API BIYOS_status BIYOS_addmdlhook(BIYOS_model *mdl,
                                        BIYOS_hook hk,
                                        void *data,
                                        BIYOS_int knd,
                                        BIYOS_int lvl,
                                        BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delmdlhook(BIYOS_model *mdl, BIYOS_index idx);

BIYOS_API BIYOS_int BIYOS_getevtknd(const BIYOS_event *evt);
BIYOS_API BIYOS_int BIYOS_getevtlvl(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevtmod(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevttext(const BIYOS_event *evt);
BIYOS_API BIYOS_index BIYOS_getnevtfld(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevtfldname(const BIYOS_event *evt, BIYOS_index idx);
BIYOS_API const char *BIYOS_getevtfldval(const BIYOS_event *evt, BIYOS_index idx);
BIYOS_API const BIYOS_model *BIYOS_getevtmdl(const BIYOS_event *evt);

BIYOS_API BIYOS_status BIYOS_fmttext(const BIYOS_event *evt, size_t cap, char *buf, size_t *len);
BIYOS_API BIYOS_status BIYOS_fmtjson(const BIYOS_event *evt, size_t cap, char *buf, size_t *len);

#define BIYOS_VARKND_CONT 1
#define BIYOS_VARKND_BIN 2
#define BIYOS_VARKND_INT 3
#define BIYOS_VARKND_SEMICONT 4
#define BIYOS_VARKND_SEMIINT 5

#define BIYOS_OBJSENSE_MIN 1
#define BIYOS_OBJSENSE_MAX 2

#define BIYOS_SOSKND_ONE 1
#define BIYOS_SOSKND_TWO 2

#define BIYOS_CONEKND_NONNEG 1
#define BIYOS_CONEKND_SOC 2
#define BIYOS_CONEKND_RSOC 3
#define BIYOS_CONEKND_EXP 4
#define BIYOS_CONEKND_DEXP 5
#define BIYOS_CONEKND_POW 6
#define BIYOS_CONEKND_DPOW 7
#define BIYOS_CONEKND_PSD 8

#define BIYOS_FILEFMT_MPS 1
#define BIYOS_FILEFMT_FIXEDMPS 2
#define BIYOS_FILEFMT_NL 3
#define BIYOS_FILEFMT_BYS 4

BIYOS_API BIYOS_status BIYOS_newmdl(BIYOS_env *env, BIYOS_model **mdl);
BIYOS_API BIYOS_status BIYOS_copymdl(const BIYOS_model *mdl, BIYOS_env *env, BIYOS_model **copy);
BIYOS_API BIYOS_status BIYOS_cmpmdl(const BIYOS_model *a, const BIYOS_model *b, BIYOS_int *same);
BIYOS_API BIYOS_status BIYOS_freemdl(BIYOS_model *mdl);
BIYOS_API BIYOS_status BIYOS_readmdl(BIYOS_env *env,
                                     const char *path,
                                     BIYOS_int fmt,
                                     BIYOS_model **mdl);
BIYOS_API BIYOS_status BIYOS_writemdl(const BIYOS_model *mdl, const char *path, BIYOS_int fmt);
BIYOS_API BIYOS_status BIYOS_setmdlname(BIYOS_model *mdl, const char *name);
BIYOS_API BIYOS_status BIYOS_getmdlname(const BIYOS_model *mdl, const char **name);
BIYOS_API BIYOS_status BIYOS_delmdlname(BIYOS_model *mdl);
BIYOS_API BIYOS_status BIYOS_addvar(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_int *knd,
                                    const BIYOS_real *lb,
                                    const BIYOS_real *ub,
                                    BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_setvarbndsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setvarbndrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getvarbndsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getvarbndrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setvarkndsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_setvarkndrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_getvarkndsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_getvarkndrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_setvartolsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *atol,
                                         const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_setvartolrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_real *atol,
                                         const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getvartolsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *atol,
                                         BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getvartolrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_real *atol,
                                         BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_delvartolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delvartolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_getnvar(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getvarsp(const BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_int *knd,
                                      BIYOS_real *lb,
                                      BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getvarrg(const BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_int *knd,
                                      BIYOS_real *lb,
                                      BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setvarnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_setvarnamerg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getvarnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getvarnamerg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getvaridx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delvarnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delvarnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delvarsp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delvarrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_addfun(BIYOS_model *mdl, BIYOS_index n, BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_getnfun(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_addcstsp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      const BIYOS_real *cst);
BIYOS_API BIYOS_status BIYOS_addcstrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      const BIYOS_real *cst);
BIYOS_API BIYOS_status BIYOS_setcstsp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      const BIYOS_real *cst);
BIYOS_API BIYOS_status BIYOS_setcstrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      const BIYOS_real *cst);
BIYOS_API BIYOS_status BIYOS_getcstsp(const BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_real *cst);
BIYOS_API BIYOS_status BIYOS_getcstrg(const BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_real *cst);
BIYOS_API BIYOS_status BIYOS_setfunnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_setfunnamerg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getfunnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getfunnamerg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getfunidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delfunnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delfunnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delfunsp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delfunrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_clrfunsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_clrfunrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_addlinterm(BIYOS_model *mdl,
                                        BIYOS_offset nnz,
                                        const BIYOS_index *fun,
                                        const BIYOS_index *var,
                                        const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addrlintermsp(BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const BIYOS_offset *cnt,
                                           const BIYOS_index *var,
                                           const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addrlintermrg(BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const BIYOS_offset *cnt,
                                           const BIYOS_index *var,
                                           const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addclintermsp(BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const BIYOS_offset *cnt,
                                           const BIYOS_index *fun,
                                           const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addclintermrg(BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const BIYOS_offset *cnt,
                                           const BIYOS_index *fun,
                                           const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getnrlintermsp(const BIYOS_model *mdl,
                                            BIYOS_index n,
                                            const BIYOS_index *idx,
                                            BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnrlintermrg(const BIYOS_model *mdl,
                                            BIYOS_index beg,
                                            BIYOS_index end,
                                            BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getrlintermsp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           BIYOS_offset cap,
                                           BIYOS_offset *cnt,
                                           BIYOS_index *var,
                                           BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getrlintermrg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           BIYOS_offset cap,
                                           BIYOS_offset *cnt,
                                           BIYOS_index *var,
                                           BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getnclintermsp(const BIYOS_model *mdl,
                                            BIYOS_index n,
                                            const BIYOS_index *idx,
                                            BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnclintermrg(const BIYOS_model *mdl,
                                            BIYOS_index beg,
                                            BIYOS_index end,
                                            BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getclintermsp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           BIYOS_offset cap,
                                           BIYOS_offset *cnt,
                                           BIYOS_index *fun,
                                           BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getclintermrg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           BIYOS_offset cap,
                                           BIYOS_offset *cnt,
                                           BIYOS_index *fun,
                                           BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addquadterm(BIYOS_model *mdl,
                                         BIYOS_offset nnz,
                                         const BIYOS_index *fun,
                                         const BIYOS_index *var1,
                                         const BIYOS_index *var2,
                                         const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addquadtermsp(BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const BIYOS_offset *cnt,
                                           const BIYOS_index *var1,
                                           const BIYOS_index *var2,
                                           const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addquadtermrg(BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const BIYOS_offset *cnt,
                                           const BIYOS_index *var1,
                                           const BIYOS_index *var2,
                                           const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getnquadtermsp(const BIYOS_model *mdl,
                                            BIYOS_index n,
                                            const BIYOS_index *idx,
                                            BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnquadtermrg(const BIYOS_model *mdl,
                                            BIYOS_index beg,
                                            BIYOS_index end,
                                            BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getquadtermsp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           BIYOS_offset cap,
                                           BIYOS_offset *cnt,
                                           BIYOS_index *var1,
                                           BIYOS_index *var2,
                                           BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getquadtermrg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           BIYOS_offset cap,
                                           BIYOS_offset *cnt,
                                           BIYOS_index *var1,
                                           BIYOS_index *var2,
                                           BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addcon(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_index *fun,
                                    const BIYOS_real *lb,
                                    const BIYOS_real *ub,
                                    BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_setconbndsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setconbndrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getconbndsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getconbndrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setcontolsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *atol,
                                         const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_setcontolrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_real *atol,
                                         const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getcontolsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *atol,
                                         BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getcontolrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_real *atol,
                                         BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_delcontolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delcontolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_getncon(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getconsp(const BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *fun,
                                      BIYOS_real *lb,
                                      BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getconrg(const BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *fun,
                                      BIYOS_real *lb,
                                      BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setconnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_setconnamerg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getconnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getconnamerg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getconidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delconnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delconnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delconsp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delconrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_addobj(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_index *fun,
                                    const BIYOS_int *sense,
                                    const BIYOS_int *prio,
                                    const BIYOS_real *coef,
                                    BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_setobjsensesp(BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const BIYOS_int *sense);
BIYOS_API BIYOS_status BIYOS_setobjsenserg(BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const BIYOS_int *sense);
BIYOS_API BIYOS_status BIYOS_getobjsensesp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           BIYOS_int *sense);
BIYOS_API BIYOS_status BIYOS_getobjsenserg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           BIYOS_int *sense);
BIYOS_API BIYOS_status BIYOS_setobjpriosp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_int *prio);
BIYOS_API BIYOS_status BIYOS_setobjpriorg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const BIYOS_int *prio);
BIYOS_API BIYOS_status BIYOS_getobjpriosp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_int *prio);
BIYOS_API BIYOS_status BIYOS_getobjpriorg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          BIYOS_int *prio);
BIYOS_API BIYOS_status BIYOS_setobjcoefsp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_setobjcoefrg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getobjcoefsp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getobjcoefrg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_setobjtolsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *atol,
                                         const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_setobjtolrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_real *atol,
                                         const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getobjtolsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *atol,
                                         BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getobjtolrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_real *atol,
                                         BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getnobj(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getobjsp(const BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *fun,
                                      BIYOS_int *sense,
                                      BIYOS_int *prio,
                                      BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getobjrg(const BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *fun,
                                      BIYOS_int *sense,
                                      BIYOS_int *prio,
                                      BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_setobjnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_setobjnamerg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getobjnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getobjnamerg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getobjidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delobjnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delobjnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delobjsp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delobjrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_addind(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_index *var,
                                    const BIYOS_int *val,
                                    const BIYOS_index *fun,
                                    const BIYOS_real *lb,
                                    const BIYOS_real *ub,
                                    BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_setindbndsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setindbndrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getindbndsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getindbndrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setindvarsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_index *var,
                                         const BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_setindvarrg(BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         const BIYOS_index *var,
                                         const BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getindvarsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_index *var,
                                         BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getindvarrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_index *var,
                                         BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getnind(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getindsp(const BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *var,
                                      BIYOS_int *val,
                                      BIYOS_index *fun,
                                      BIYOS_real *lb,
                                      BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getindrg(const BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *var,
                                      BIYOS_int *val,
                                      BIYOS_index *fun,
                                      BIYOS_real *lb,
                                      BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setindnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_setindnamerg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getindnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getindnamerg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getindidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delindnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delindnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delindsp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delindrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_addsos(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_int *knd,
                                    const BIYOS_offset *cnt,
                                    const BIYOS_index *var,
                                    const BIYOS_real *coef,
                                    BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_getnsos(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getnsosvarsp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnsosvarrg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getsossp(const BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_offset cap,
                                      BIYOS_int *knd,
                                      BIYOS_offset *cnt,
                                      BIYOS_index *var,
                                      BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getsosrg(const BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_offset cap,
                                      BIYOS_int *knd,
                                      BIYOS_offset *cnt,
                                      BIYOS_index *var,
                                      BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_setsosnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_setsosnamerg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getsosnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getsosnamerg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getsosidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delsosnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delsosnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delsossp(BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delsosrg(BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_addpwl(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_offset *cnt,
                                    const BIYOS_real *x,
                                    const BIYOS_real *y,
                                    BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_getnpwl(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getnpwlptsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnpwlptrg(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index end,
                                         BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getpwlsp(const BIYOS_model *mdl,
                                      BIYOS_index n,
                                      const BIYOS_index *idx,
                                      BIYOS_offset cap,
                                      BIYOS_offset *cnt,
                                      BIYOS_real *x,
                                      BIYOS_real *y);
BIYOS_API BIYOS_status BIYOS_getpwlrg(const BIYOS_model *mdl,
                                      BIYOS_index beg,
                                      BIYOS_index end,
                                      BIYOS_offset cap,
                                      BIYOS_offset *cnt,
                                      BIYOS_real *x,
                                      BIYOS_real *y);
BIYOS_API BIYOS_status BIYOS_addpwlnode(BIYOS_model *mdl,
                                        BIYOS_index pwl,
                                        BIYOS_offset arg,
                                        BIYOS_offset *node);
BIYOS_API BIYOS_status BIYOS_addcone(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_int *knd,
                                     const BIYOS_offset *cnt,
                                     const BIYOS_index *fun,
                                     const BIYOS_offset *prmcnt,
                                     const BIYOS_real *prm,
                                     BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_setconetolsp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_real *atol,
                                          const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_setconetolrg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const BIYOS_real *atol,
                                          const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getconetolsp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_real *atol,
                                          BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getconetolrg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          BIYOS_real *atol,
                                          BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_delconetolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delconetolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_getncone(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getnconefunsp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnconefunrg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getconesp(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       BIYOS_offset cap,
                                       BIYOS_int *knd,
                                       BIYOS_offset *cnt,
                                       BIYOS_index *fun);
BIYOS_API BIYOS_status BIYOS_getconerg(const BIYOS_model *mdl,
                                       BIYOS_index beg,
                                       BIYOS_index end,
                                       BIYOS_offset cap,
                                       BIYOS_int *knd,
                                       BIYOS_offset *cnt,
                                       BIYOS_index *fun);
BIYOS_API BIYOS_status BIYOS_getnconeprmsp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnconeprmrg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getconeprmsp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_offset cap,
                                          BIYOS_offset *prmcnt,
                                          BIYOS_real *prm);
BIYOS_API BIYOS_status BIYOS_getconeprmrg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          BIYOS_offset cap,
                                          BIYOS_offset *prmcnt,
                                          BIYOS_real *prm);
BIYOS_API BIYOS_status BIYOS_setconenamesp(BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const char *const *name);
BIYOS_API BIYOS_status BIYOS_setconenamerg(BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const char *const *name);
BIYOS_API BIYOS_status BIYOS_getconenamesp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const char **name);
BIYOS_API BIYOS_status BIYOS_getconenamerg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const char **name);
BIYOS_API BIYOS_status BIYOS_getconeidx(const BIYOS_model *mdl,
                                        BIYOS_index n,
                                        const char *const *name,
                                        BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delconenamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delconenamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delconesp(BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delconerg(BIYOS_model *mdl,
                                       BIYOS_index beg,
                                       BIYOS_index end,
                                       BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_addccon(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *fun1,
                                     const BIYOS_index *fun2,
                                     const BIYOS_real *lb,
                                     const BIYOS_real *ub,
                                     BIYOS_index *pos);
BIYOS_API BIYOS_status BIYOS_setcconbndsp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_real *lb,
                                          const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setcconbndrg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const BIYOS_real *lb,
                                          const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getcconbndsp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_real *lb,
                                          BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getcconbndrg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          BIYOS_real *lb,
                                          BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setccontolsp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_real *atol,
                                          const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_setccontolrg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const BIYOS_real *atol,
                                          const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getccontolsp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_real *atol,
                                          BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getccontolrg(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          BIYOS_real *atol,
                                          BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_delccontolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delccontolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_getnccon(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getcconsp(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       BIYOS_index *fun1,
                                       BIYOS_index *fun2,
                                       BIYOS_real *lb,
                                       BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getcconrg(const BIYOS_model *mdl,
                                       BIYOS_index beg,
                                       BIYOS_index end,
                                       BIYOS_index *fun1,
                                       BIYOS_index *fun2,
                                       BIYOS_real *lb,
                                       BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setcconnamesp(BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const char *const *name);
BIYOS_API BIYOS_status BIYOS_setcconnamerg(BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const char *const *name);
BIYOS_API BIYOS_status BIYOS_getcconnamesp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const char **name);
BIYOS_API BIYOS_status BIYOS_getcconnamerg(const BIYOS_model *mdl,
                                           BIYOS_index beg,
                                           BIYOS_index end,
                                           const char **name);
BIYOS_API BIYOS_status BIYOS_getcconidx(const BIYOS_model *mdl,
                                        BIYOS_index n,
                                        const char *const *name,
                                        BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delcconnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delcconnamerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end);
BIYOS_API BIYOS_status BIYOS_delcconsp(BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delcconrg(BIYOS_model *mdl,
                                       BIYOS_index beg,
                                       BIYOS_index end,
                                       BIYOS_index *map);

#define BIYOS_OP_CONST 1
#define BIYOS_OP_VAR 2
#define BIYOS_OP_SUM 3
#define BIYOS_OP_PROD 4
#define BIYOS_OP_NEG 5
#define BIYOS_OP_DIV 6
#define BIYOS_OP_CPOW 7
#define BIYOS_OP_POW 8
#define BIYOS_OP_EXP 9
#define BIYOS_OP_LOG 10
#define BIYOS_OP_SQRT 11
#define BIYOS_OP_SIN 12
#define BIYOS_OP_COS 13
#define BIYOS_OP_TAN 14
#define BIYOS_OP_ABS 15
#define BIYOS_OP_MAX 16
#define BIYOS_OP_MIN 17
#define BIYOS_OP_ASIN 18
#define BIYOS_OP_ACOS 19
#define BIYOS_OP_ATAN 20
#define BIYOS_OP_SINH 21
#define BIYOS_OP_COSH 22
#define BIYOS_OP_TANH 23
#define BIYOS_OP_ASINH 24
#define BIYOS_OP_ACOSH 25
#define BIYOS_OP_ATANH 26
#define BIYOS_OP_ENT 27
#define BIYOS_OP_SPOW 28
#define BIYOS_OP_ERF 29
#define BIYOS_OP_NODE 30
#define BIYOS_OP_PWL 31
#define BIYOS_OP_KEEP 32
#define BIYOS_OP_REF 33

BIYOS_API BIYOS_status BIYOS_addvarnode(BIYOS_model *mdl, BIYOS_index var, BIYOS_offset *node);
BIYOS_API BIYOS_status BIYOS_addnode(BIYOS_model *mdl,
                                     BIYOS_int op,
                                     BIYOS_offset n,
                                     const BIYOS_offset *arg,
                                     const BIYOS_real *val,
                                     BIYOS_offset *node);
BIYOS_API BIYOS_status BIYOS_addexpr(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_offset *cnt,
                                     const BIYOS_int *code,
                                     const BIYOS_offset *arg,
                                     const BIYOS_real *val,
                                     BIYOS_offset *node);
BIYOS_API BIYOS_status BIYOS_attachexprsp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_offset *node);
BIYOS_API BIYOS_status BIYOS_attachexprrg(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index end,
                                          const BIYOS_offset *node);
BIYOS_API BIYOS_status BIYOS_getnexprsp(const BIYOS_model *mdl,
                                        BIYOS_index n,
                                        const BIYOS_index *idx,
                                        BIYOS_offset *nnz,
                                        BIYOS_offset *nval);
BIYOS_API BIYOS_status BIYOS_getnexprrg(const BIYOS_model *mdl,
                                        BIYOS_index beg,
                                        BIYOS_index end,
                                        BIYOS_offset *nnz,
                                        BIYOS_offset *nval);
BIYOS_API BIYOS_status BIYOS_getexprsp(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       BIYOS_offset cap,
                                       BIYOS_offset valcap,
                                       BIYOS_offset *cnt,
                                       BIYOS_int *code,
                                       BIYOS_offset *arg,
                                       BIYOS_real *val);
BIYOS_API BIYOS_status BIYOS_getexprrg(const BIYOS_model *mdl,
                                       BIYOS_index beg,
                                       BIYOS_index end,
                                       BIYOS_offset cap,
                                       BIYOS_offset valcap,
                                       BIYOS_offset *cnt,
                                       BIYOS_int *code,
                                       BIYOS_offset *arg,
                                       BIYOS_real *val);

typedef struct BIYOS_cbreq
{
    size_t size;
    const BIYOS_model *mdl;
    BIYOS_index cb;
    BIYOS_index thread;
    const BIYOS_real *x;
    const BIYOS_real *w;
    const BIYOS_real *v;
} BIYOS_cbreq;

typedef int (*BIYOS_cbf)(const BIYOS_cbreq *req, BIYOS_real *f, void *data);
typedef int (*BIYOS_cbg)(const BIYOS_cbreq *req, BIYOS_real *g, void *data);
typedef int (*BIYOS_cbh)(const BIYOS_cbreq *req, BIYOS_real *h, void *data);
typedef int (*BIYOS_cbhv)(const BIYOS_cbreq *req, BIYOS_real *hv, void *data);

BIYOS_API BIYOS_status BIYOS_addcb(BIYOS_model *mdl,
                                   BIYOS_index m,
                                   const BIYOS_index *fun,
                                   BIYOS_index n,
                                   const BIYOS_index *var,
                                   void *data,
                                   BIYOS_index *cb);
BIYOS_API BIYOS_status BIYOS_setcbf(BIYOS_model *mdl, BIYOS_index cb, BIYOS_cbf f);
BIYOS_API BIYOS_status BIYOS_setcbg(BIYOS_model *mdl,
                                    BIYOS_index cb,
                                    BIYOS_offset nnz,
                                    const BIYOS_index *fun,
                                    const BIYOS_index *var,
                                    BIYOS_cbg g);
BIYOS_API BIYOS_status BIYOS_setcbh(BIYOS_model *mdl,
                                    BIYOS_index cb,
                                    BIYOS_offset nnz,
                                    const BIYOS_index *var1,
                                    const BIYOS_index *var2,
                                    BIYOS_cbh h);
BIYOS_API BIYOS_status BIYOS_setcbhv(BIYOS_model *mdl, BIYOS_index cb, BIYOS_cbhv hv);
BIYOS_API BIYOS_status BIYOS_setcbdata(BIYOS_model *mdl, BIYOS_index cb, void *data);
BIYOS_API BIYOS_status BIYOS_getcbdata(const BIYOS_model *mdl, BIYOS_index cb, void **data);
BIYOS_API BIYOS_status BIYOS_getncbfun(const BIYOS_model *mdl, BIYOS_index cb, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getcbfun(const BIYOS_model *mdl,
                                      BIYOS_index cb,
                                      BIYOS_index cap,
                                      BIYOS_index *fun);
BIYOS_API BIYOS_status BIYOS_getncbvar(const BIYOS_model *mdl, BIYOS_index cb, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getcbvar(const BIYOS_model *mdl,
                                      BIYOS_index cb,
                                      BIYOS_index cap,
                                      BIYOS_index *var);
BIYOS_API BIYOS_status BIYOS_delcbsp(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *idx,
                                     BIYOS_index *map);
BIYOS_API BIYOS_status BIYOS_delcbrg(BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index end,
                                     BIYOS_index *map);

BIYOS_API BIYOS_status BIYOS_evalf(BIYOS_model *mdl, const BIYOS_real *x, BIYOS_real *f);
BIYOS_API BIYOS_status BIYOS_evalg(BIYOS_model *mdl, const BIYOS_real *x, BIYOS_real *g);
BIYOS_API BIYOS_status BIYOS_evalh(BIYOS_model *mdl,
                                   const BIYOS_real *x,
                                   const BIYOS_real *w,
                                   BIYOS_real *h);
BIYOS_API BIYOS_status BIYOS_evalhv(BIYOS_model *mdl,
                                    const BIYOS_real *x,
                                    const BIYOS_real *w,
                                    const BIYOS_real *v,
                                    BIYOS_real *hv);
BIYOS_API BIYOS_status BIYOS_getngpatsp(BIYOS_model *mdl,
                                        BIYOS_index n,
                                        const BIYOS_index *idx,
                                        BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getngpatrg(BIYOS_model *mdl,
                                        BIYOS_index beg,
                                        BIYOS_index end,
                                        BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getgpatsp(BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       BIYOS_offset cap,
                                       BIYOS_offset *cnt,
                                       BIYOS_index *var);
BIYOS_API BIYOS_status BIYOS_getgpatrg(BIYOS_model *mdl,
                                       BIYOS_index beg,
                                       BIYOS_index end,
                                       BIYOS_offset cap,
                                       BIYOS_offset *cnt,
                                       BIYOS_index *var);
BIYOS_API BIYOS_status BIYOS_getnhpatsp(BIYOS_model *mdl,
                                        BIYOS_index n,
                                        const BIYOS_index *idx,
                                        BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_getnhpatrg(BIYOS_model *mdl,
                                        BIYOS_index beg,
                                        BIYOS_index end,
                                        BIYOS_offset *nnz);
BIYOS_API BIYOS_status BIYOS_gethpatsp(BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       BIYOS_offset cap,
                                       BIYOS_offset *cnt,
                                       BIYOS_index *var);
BIYOS_API BIYOS_status BIYOS_gethpatrg(BIYOS_model *mdl,
                                       BIYOS_index beg,
                                       BIYOS_index end,
                                       BIYOS_offset cap,
                                       BIYOS_offset *cnt,
                                       BIYOS_index *var);

#define BIYOS_TRM_NONE 0
#define BIYOS_TRM_OPTIMAL 101
#define BIYOS_TRM_LOCAL 102
#define BIYOS_TRM_NEARLOCAL 103
#define BIYOS_TRM_ITERLIM 201
#define BIYOS_TRM_TIMELIM 202
#define BIYOS_TRM_CPULIM 203
#define BIYOS_TRM_EVALLIM 204
#define BIYOS_TRM_STOP 205
#define BIYOS_TRM_INFEAS 301
#define BIYOS_TRM_LOCINFEAS 302
#define BIYOS_TRM_UNBND 303
#define BIYOS_TRM_SLOW 401
#define BIYOS_TRM_NUMERIC 402

#define BIYOS_RES_NONE 0
#define BIYOS_RES_FEAS 1
#define BIYOS_RES_NEARFEAS 2
#define BIYOS_RES_INFEAS 3
#define BIYOS_RES_CER 4
#define BIYOS_RES_NEARCER 5
#define BIYOS_RES_UNKNOWN 6

#define BIYOS_CLM_NONE 0
#define BIYOS_CLM_LOCAL 1
#define BIYOS_CLM_GLOBAL 2

#ifdef __cplusplus
}
#endif
