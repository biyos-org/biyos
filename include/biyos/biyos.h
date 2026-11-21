#pragma once

#include "biyos/events.h"
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
#define BIYOS_ERR_INVALIDARG (-100)
#define BIYOS_ERR_NOMEMORY (-400)
#define BIYOS_ERR_INTERNAL (-500)

BIYOS_API void BIYOS_version(BIYOS_int *major, BIYOS_int *minor, BIYOS_int *patch);

BIYOS_API BIYOS_status BIYOS_newenv(BIYOS_env **env);
BIYOS_API BIYOS_status BIYOS_freeenv(BIYOS_env *env);
BIYOS_API const char *BIYOS_lasterror(const BIYOS_env *env);

BIYOS_API BIYOS_status BIYOS_getoptid(BIYOS_env *env, const char *name, BIYOS_int *opt);
BIYOS_API BIYOS_status BIYOS_getoptkind(BIYOS_env *env, BIYOS_int opt, BIYOS_int *kind);
BIYOS_API BIYOS_status BIYOS_getoptversions(BIYOS_env *env,
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

BIYOS_API BIYOS_status BIYOS_addhook(BIYOS_env *env,
                                     BIYOS_hook hook,
                                     void *data,
                                     BIYOS_int kind,
                                     BIYOS_int lowest,
                                     BIYOS_index *hookid);
BIYOS_API BIYOS_status BIYOS_delhook(BIYOS_env *env, BIYOS_index hookid);

BIYOS_API BIYOS_int BIYOS_getevtkind(const BIYOS_event *evt);
BIYOS_API BIYOS_int BIYOS_getevtlvl(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevtmodule(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevttext(const BIYOS_event *evt);
BIYOS_API BIYOS_index BIYOS_getnevtfields(const BIYOS_event *evt);
BIYOS_API const char *BIYOS_getevtfieldname(const BIYOS_event *evt, BIYOS_index field);
BIYOS_API const char *BIYOS_getevtfieldvalue(const BIYOS_event *evt, BIYOS_index field);

BIYOS_API BIYOS_status BIYOS_fmttext(const BIYOS_event *evt,
                                     size_t size,
                                     char *buf,
                                     size_t *needed);
BIYOS_API BIYOS_status BIYOS_fmtjson(const BIYOS_event *evt,
                                     size_t size,
                                     char *buf,
                                     size_t *needed);

BIYOS_API BIYOS_status BIYOS_newmodel(BIYOS_env *env, BIYOS_model **mdl);
BIYOS_API BIYOS_status BIYOS_copymodel(BIYOS_model *mdl, BIYOS_env *env, BIYOS_model **copy);
BIYOS_API BIYOS_status BIYOS_freemodel(BIYOS_model *mdl);
BIYOS_API BIYOS_status BIYOS_addvars(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_int *kinds,
                                     const BIYOS_real *lb,
                                     const BIYOS_real *ub,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setvarbounds(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_real *lb,
                                          const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_setvarkinds(BIYOS_model *mdl,
                                         BIYOS_index n,
                                         const BIYOS_index *idx,
                                         const BIYOS_int *kinds);
BIYOS_API BIYOS_status BIYOS_getnvars(BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getvars(BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_int *kinds,
                                     BIYOS_real *lb,
                                     BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_addfuns(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_real *consts,
                                     const BIYOS_offset *starts,
                                     const BIYOS_index *varidx,
                                     const BIYOS_real *vals,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_getnfuns(BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getfuns(BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_offset size,
                                     BIYOS_real *consts,
                                     BIYOS_offset *starts,
                                     BIYOS_offset *needed,
                                     BIYOS_index *varidx,
                                     BIYOS_real *vals);
BIYOS_API BIYOS_status BIYOS_addquadterms(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *funs,
                                          const BIYOS_offset *starts,
                                          const BIYOS_index *vars1,
                                          const BIYOS_index *vars2,
                                          const BIYOS_real *vals);
BIYOS_API BIYOS_status BIYOS_getquadterms(BIYOS_model *mdl,
                                          BIYOS_index beg,
                                          BIYOS_index n,
                                          BIYOS_offset size,
                                          BIYOS_offset *starts,
                                          BIYOS_offset *needed,
                                          BIYOS_index *vars1,
                                          BIYOS_index *vars2,
                                          BIYOS_real *vals);
BIYOS_API BIYOS_status BIYOS_addcons(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *funs,
                                     const BIYOS_real *lb,
                                     const BIYOS_real *ub,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setconbounds(BIYOS_model *mdl,
                                          BIYOS_index n,
                                          const BIYOS_index *idx,
                                          const BIYOS_real *lb,
                                          const BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_getncons(BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getcons(BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_index *funs,
                                     BIYOS_real *lb,
                                     BIYOS_real *ub);
BIYOS_API BIYOS_status BIYOS_addobjs(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *funs,
                                     const BIYOS_int *senses,
                                     const BIYOS_int *prios,
                                     const BIYOS_real *weights,
                                     const BIYOS_real *abstol,
                                     const BIYOS_real *reltol,
                                     BIYOS_index *beg);
BIYOS_API BIYOS_status BIYOS_setobjs(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     const BIYOS_index *idx,
                                     const BIYOS_int *senses,
                                     const BIYOS_int *prios,
                                     const BIYOS_real *weights,
                                     const BIYOS_real *abstol,
                                     const BIYOS_real *reltol);
BIYOS_API BIYOS_status BIYOS_getnobjs(BIYOS_model *mdl, BIYOS_index *n);
BIYOS_API BIYOS_status BIYOS_getobjs(BIYOS_model *mdl,
                                     BIYOS_index beg,
                                     BIYOS_index n,
                                     BIYOS_index *funs,
                                     BIYOS_int *senses,
                                     BIYOS_int *prios,
                                     BIYOS_real *weights,
                                     BIYOS_real *abstol,
                                     BIYOS_real *reltol);

#ifdef __cplusplus
}
#endif
