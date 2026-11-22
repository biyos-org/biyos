#pragma once

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
typedef struct BIYOS_env BIYOS_env;
typedef struct BIYOS_event BIYOS_event;
typedef struct BIYOS_model BIYOS_model;
typedef void (*BIYOS_hook)(const BIYOS_event *evt, void *data);

#define BIYOS_OK 0
#define BIYOS_ERR_BADARG (-100)
#define BIYOS_ERR_NOMEMORY (-400)
#define BIYOS_ERR_INTERNAL (-500)

BIYOS_API void BIYOS_version(BIYOS_int *major, BIYOS_int *minor, BIYOS_int *patch);

BIYOS_API BIYOS_status BIYOS_newenv(BIYOS_env **env);
BIYOS_API BIYOS_status BIYOS_freeenv(BIYOS_env *env);
BIYOS_API const char *BIYOS_lasterror(const BIYOS_env *env);
BIYOS_API BIYOS_status BIYOS_interrupt(BIYOS_env *env);

#define BIYOS_OPT_SEED 1
#define BIYOS_OPT_MAXTIME 2
#define BIYOS_OPT_CHECKLVL 3
#define BIYOS_OPT_PRESOLVE 4
#define BIYOS_OPT_REPRO 5
#define BIYOS_OPT_FEASTOLABS 6
#define BIYOS_OPT_FEASTOLREL 7
#define BIYOS_OPT_INTTOL 8
#define BIYOS_OPT_ZEROTOL 9
#define BIYOS_OPT_MAXCPUTIME 10

#define BIYOS_KND_BOOL 1
#define BIYOS_KND_INT 2
#define BIYOS_KND_DBL 3
#define BIYOS_KND_STR 4
#define BIYOS_KND_ENUM 5

BIYOS_API BIYOS_status BIYOS_getoptid(BIYOS_env *env, const char *name, BIYOS_int *opt);
BIYOS_API BIYOS_status BIYOS_getoptknd(BIYOS_env *env, BIYOS_int opt, BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_getoptver(BIYOS_env *env,
                                       BIYOS_int opt,
                                       BIYOS_int *since,
                                       BIYOS_int *deprecated,
                                       BIYOS_int *retired);
BIYOS_API BIYOS_status BIYOS_setboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val);
BIYOS_API BIYOS_status BIYOS_setintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val);
BIYOS_API BIYOS_status BIYOS_setdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real val);
BIYOS_API BIYOS_status BIYOS_setstropt(BIYOS_env *env, BIYOS_int opt, const char *val);
BIYOS_API BIYOS_status BIYOS_getboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val);
BIYOS_API BIYOS_status BIYOS_getdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real *val);
BIYOS_API BIYOS_status BIYOS_getstropt(BIYOS_env *env, BIYOS_int opt, const char **val);

#define BIYOS_EVT_MESSAGE 1
#define BIYOS_EVT_CHECK 2
#define BIYOS_EVT_PHASE 3

#define BIYOS_LVL_DBG 1
#define BIYOS_LVL_INFO 2
#define BIYOS_LVL_WARN 3
#define BIYOS_LVL_ERR 4

BIYOS_API BIYOS_status BIYOS_addhook(BIYOS_env *env,
                                     BIYOS_hook hk,
                                     void *data,
                                     BIYOS_int knd,
                                     BIYOS_int lvl,
                                     BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delhook(BIYOS_env *env, BIYOS_index idx);

BIYOS_API BIYOS_int BIYOS_getevtknd(const BIYOS_event *evt);
BIYOS_API BIYOS_int BIYOS_getevtlvl(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevtmod(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevttext(const BIYOS_event *evt);
BIYOS_API BIYOS_index BIYOS_getnevtfld(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevtfldname(const BIYOS_event *evt, BIYOS_index idx);
BIYOS_API const char *BIYOS_getevtfldval(const BIYOS_event *evt, BIYOS_index idx);

BIYOS_API BIYOS_status BIYOS_fmttext(const BIYOS_event *evt, size_t size, char *buf, size_t *len);
BIYOS_API BIYOS_status BIYOS_fmtjson(const BIYOS_event *evt, size_t size, char *buf, size_t *len);

#define BIYOS_VAR_CONT 1
#define BIYOS_VAR_BIN 2
#define BIYOS_VAR_INT 3
#define BIYOS_VAR_SEMICONT 4
#define BIYOS_VAR_SEMIINT 5

#define BIYOS_MINIMIZE 1
#define BIYOS_MAXIMIZE 2

BIYOS_API BIYOS_status BIYOS_newmdl(BIYOS_env *env, BIYOS_model **mdl);
BIYOS_API BIYOS_status BIYOS_copymdl(const BIYOS_model *mdl, BIYOS_env *env, BIYOS_model **copy);
BIYOS_API BIYOS_status BIYOS_freemdl(BIYOS_model *mdl);
BIYOS_API BIYOS_status BIYOS_addvar(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_int *knd,
                                    const BIYOS_real *lb,
                                    const BIYOS_real *ub,
                                    BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setvarbndsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getvarbndsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setvarkndsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_getvarkndsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_getnvar(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getvars(const BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_int *knd,
                                     BIYOS_real *lb,
                                     BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_addfuns(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_real *cst,
                                     const BIYOS_offset *off,
                                     const BIYOS_index *varidx,
                                     const BIYOS_real *coef,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_getnfun(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getfuns(const BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_offset size,
                                     BIYOS_real *cst,
                                     BIYOS_offset *off,
                                     BIYOS_offset *nnz,
                                     BIYOS_index *varidx,
                                     BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addlinterms(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *funidx,
                                         const BIYOS_offset *off,
                                         const BIYOS_index *varidx,
                                         const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getvarterms(const BIYOS_model *mdl,
                                         BIYOS_index beg,
                                         BIYOS_index n,
                                         BIYOS_offset size,
                                         BIYOS_offset *off,
                                         BIYOS_offset *nnz,
                                         BIYOS_index *funidx,
                                         BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addquadterms(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *funidx,
                                          const BIYOS_offset *off,
                                          const BIYOS_index *varidx1,
                                          const BIYOS_index *varidx2,
                                          const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getquadterms(const BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index n,
                                          BIYOS_offset size,
                                          BIYOS_offset *off,
                                          BIYOS_offset *nnz,
                                          BIYOS_index *varidx1,
                                          BIYOS_index *varidx2,
                                          BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_addcon(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_index *funidx,
                                    const BIYOS_real *lb,
                                    const BIYOS_real *ub,
                                    BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setconbndsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *lb,
                                         const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getconbndsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *lb,
                                         BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getncon(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getcons(const BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_index *funidx,
                                     BIYOS_real *lb,
                                     BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_addobj(BIYOS_model *mdl,
                                    BIYOS_index n,
                                    const BIYOS_index *funidx,
                                    const BIYOS_int *sense,
                                    const BIYOS_int *prio,
                                    const BIYOS_real *coef,
                                    BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setobjsensesp(BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           const BIYOS_int *sense);
BIYOS_API BIYOS_status BIYOS_getobjsensesp(const BIYOS_model *mdl,
                                           BIYOS_index n,
                                           const BIYOS_index *idx,
                                           BIYOS_int *sense);
BIYOS_API BIYOS_status BIYOS_setobjpriosp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_int *prio);
BIYOS_API BIYOS_status BIYOS_getobjpriosp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_int *prio);
BIYOS_API BIYOS_status BIYOS_setobjcoefsp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_getobjcoefsp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_setobjtolsp(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_real *atol,
                                         const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getobjtolsp(const BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         BIYOS_real *atol,
                                         BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getnobj(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getobjs(const BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_index *funidx,
                                     BIYOS_int *sense,
                                     BIYOS_int *prio,
                                     BIYOS_real *coef);
BIYOS_API BIYOS_status BIYOS_setmdlname(BIYOS_model *mdl, const char *name);
BIYOS_API BIYOS_status BIYOS_getmdlname(const BIYOS_model *mdl, const char **name);
BIYOS_API BIYOS_status BIYOS_delmdlname(BIYOS_model *mdl);
BIYOS_API BIYOS_status BIYOS_setvarnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getvarnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getvaridx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delvarnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_setfunnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getfunnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getfunidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delfunnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_setconnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getconnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getconidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delconnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_setobjnamesp(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char *const *name);
BIYOS_API BIYOS_status BIYOS_getobjnamesp(const BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const char **name);
BIYOS_API BIYOS_status BIYOS_getobjidx(const BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const char *const *name,
                                       BIYOS_index *idx);
BIYOS_API BIYOS_status BIYOS_delobjnamesp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx);

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
