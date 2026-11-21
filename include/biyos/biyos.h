#pragma once

#include "biyos/expr.h"
#include "biyos/model.h"
#include "biyos/options.h"
#include "biyos/status.h"

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
BIYOS_API BIYOS_index BIYOS_getnevtflds(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevtfldname(const BIYOS_event *evt, BIYOS_index idx);
BIYOS_API const char *BIYOS_getevtfldval(const BIYOS_event *evt, BIYOS_index idx);

BIYOS_API BIYOS_status BIYOS_fmttext(const BIYOS_event *evt, size_t size, char *buf, size_t *len);
BIYOS_API BIYOS_status BIYOS_fmtjson(const BIYOS_event *evt, size_t size, char *buf, size_t *len);

BIYOS_API BIYOS_status BIYOS_newmdl(BIYOS_env *env, BIYOS_model **mdl);
BIYOS_API BIYOS_status BIYOS_copymdl(const BIYOS_model *mdl, BIYOS_env *env, BIYOS_model **copy);
BIYOS_API BIYOS_status BIYOS_freemdl(BIYOS_model *mdl);
BIYOS_API BIYOS_status BIYOS_addvars(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_int *knd,
                                     const BIYOS_real *lb,
                                     const BIYOS_real *ub,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setvarbnd(BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       const BIYOS_real *lb,
                                       const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setvarknd(BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       const BIYOS_int *knd);
BIYOS_API BIYOS_status BIYOS_getnvars(const BIYOS_model *mdl, BIYOS_index *n);
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
BIYOS_API BIYOS_status BIYOS_getnfuns(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getfuns(const BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_offset size,
                                     BIYOS_real *cst,
                                     BIYOS_offset *off,
                                     BIYOS_offset *nnz,
                                     BIYOS_index *varidx,
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
BIYOS_API BIYOS_status BIYOS_addcons(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *funidx,
                                     const BIYOS_real *lb,
                                     const BIYOS_real *ub,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setconbnd(BIYOS_model *mdl,
                                       BIYOS_index n,
                                       const BIYOS_index *idx,
                                       const BIYOS_real *lb,
                                       const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getncons(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getcons(const BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_index *funidx,
                                     BIYOS_real *lb,
                                     BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_addobjs(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *funidx,
                                     const BIYOS_int *sense,
                                     const BIYOS_int *prio,
                                     const BIYOS_real *coef,
                                     const BIYOS_real *atol,
                                     const BIYOS_real *rtol,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setobjs(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *idx,
                                     const BIYOS_int *sense,
                                     const BIYOS_int *prio,
                                     const BIYOS_real *coef,
                                     const BIYOS_real *atol,
                                     const BIYOS_real *rtol);
BIYOS_API BIYOS_status BIYOS_getnobjs(const BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getobjs(const BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_index *funidx,
                                     BIYOS_int *sense,
                                     BIYOS_int *prio,
                                     BIYOS_real *coef,
                                     BIYOS_real *atol,
                                     BIYOS_real *rtol);

#ifdef __cplusplus
}
#endif
