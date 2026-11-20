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
typedef struct BIYOS_env BIYOS_env;
typedef struct BIYOS_evt BIYOS_evt;
typedef struct BIYOS_model BIYOS_model;
typedef void (*BIYOS_hook)(const BIYOS_evt *event, void *data);

#define BIYOS_OK 0
#define BIYOS_ERR_INVALIDARG (-100)
#define BIYOS_ERR_NOMEMORY (-400)
#define BIYOS_ERR_INTERNAL (-500)

BIYOS_API void BIYOS_version(int *major, int *minor, int *patch);

BIYOS_API BIYOS_status BIYOS_newenv(BIYOS_env **env);
BIYOS_API BIYOS_status BIYOS_freeenv(BIYOS_env *env);
BIYOS_API const char *BIYOS_lasterror(const BIYOS_env *env);

BIYOS_API BIYOS_status BIYOS_getoptid(BIYOS_env *env, const char *name, int *option);
BIYOS_API BIYOS_status BIYOS_getoptkind(BIYOS_env *env, int option, int *kind);
BIYOS_API BIYOS_status BIYOS_getoptversions(BIYOS_env *env,
                                            int option,
                                            int *since,
                                            int *deprecated,
                                            int *retired);
BIYOS_API BIYOS_status BIYOS_setboolopt(BIYOS_env *env, int option, int value);
BIYOS_API BIYOS_status BIYOS_setintopt(BIYOS_env *env, int option, int value);
BIYOS_API BIYOS_status BIYOS_setdblopt(BIYOS_env *env, int option, double value);
BIYOS_API BIYOS_status BIYOS_setstropt(BIYOS_env *env, int option, const char *value);
BIYOS_API BIYOS_status BIYOS_getboolopt(BIYOS_env *env, int option, int *value);
BIYOS_API BIYOS_status BIYOS_getintopt(BIYOS_env *env, int option, int *value);
BIYOS_API BIYOS_status BIYOS_getdblopt(BIYOS_env *env, int option, double *value);
BIYOS_API BIYOS_status BIYOS_getstropt(BIYOS_env *env, int option, const char **value);

BIYOS_API BIYOS_status BIYOS_addhook(BIYOS_env *env,
                                     BIYOS_hook subscriber,
                                     void *data,
                                     int kind,
                                     int lowest,
                                     int *subscription);
BIYOS_API BIYOS_status BIYOS_delhook(BIYOS_env *env, int subscription);

BIYOS_API int BIYOS_getevtkind(const BIYOS_evt *event);
BIYOS_API int BIYOS_getevtlvl(const BIYOS_evt *event);
BIYOS_API const char *BIYOS_getevtmodule(const BIYOS_evt *event);
BIYOS_API const char *BIYOS_getevttext(const BIYOS_evt *event);
BIYOS_API int BIYOS_getnevtfields(const BIYOS_evt *event);
BIYOS_API const char *BIYOS_getevtfieldname(const BIYOS_evt *event, int field);
BIYOS_API const char *BIYOS_getevtfieldvalue(const BIYOS_evt *event, int field);

BIYOS_API BIYOS_status BIYOS_fmttext(const BIYOS_evt *event,
                                     char *buffer,
                                     size_t size,
                                     size_t *needed);
BIYOS_API BIYOS_status BIYOS_fmtjson(const BIYOS_evt *event,
                                     char *buffer,
                                     size_t size,
                                     size_t *needed);

BIYOS_API BIYOS_status BIYOS_newmodel(BIYOS_env *env, BIYOS_model **model);
BIYOS_API BIYOS_status BIYOS_copymodel(BIYOS_model *model, BIYOS_env *env, BIYOS_model **copy);
BIYOS_API BIYOS_status BIYOS_freemodel(BIYOS_model *model);
BIYOS_API BIYOS_status BIYOS_addvars(BIYOS_model *model,
                                     int count,
                                     const int *kinds,
                                     const double *lower,
                                     const double *upper,
                                     int *first);
BIYOS_API BIYOS_status BIYOS_setvarbounds(BIYOS_model *model,
                                          int count,
                                          const int *indices,
                                          const double *lower,
                                          const double *upper);
BIYOS_API BIYOS_status BIYOS_setvarkinds(BIYOS_model *model,
                                         int count,
                                         const int *indices,
                                         const int *kinds);
BIYOS_API BIYOS_status BIYOS_getnvars(BIYOS_model *model, int *count);
BIYOS_API BIYOS_status BIYOS_getvars(BIYOS_model *model,
                                     int first,
                                     int count,
                                     int *kinds,
                                     double *lower,
                                     double *upper);
BIYOS_API BIYOS_status BIYOS_addfuns(BIYOS_model *model,
                                     int count,
                                     const double *constants,
                                     const int64_t *starts,
                                     const int *variables,
                                     const double *values,
                                     int *first);
BIYOS_API BIYOS_status BIYOS_getnfuns(BIYOS_model *model, int *count);
BIYOS_API BIYOS_status BIYOS_getfuns(BIYOS_model *model,
                                     int first,
                                     int count,
                                     double *constants,
                                     int64_t *starts,
                                     int64_t size,
                                     int64_t *needed,
                                     int *variables,
                                     double *values);
BIYOS_API BIYOS_status BIYOS_addquadterms(BIYOS_model *model,
                                          int count,
                                          const int *functions,
                                          const int64_t *starts,
                                          const int *firsts,
                                          const int *seconds,
                                          const double *values);
BIYOS_API BIYOS_status BIYOS_getquadterms(BIYOS_model *model,
                                          int first,
                                          int count,
                                          int64_t *starts,
                                          int64_t size,
                                          int64_t *needed,
                                          int *firsts,
                                          int *seconds,
                                          double *values);
BIYOS_API BIYOS_status BIYOS_addcons(BIYOS_model *model,
                                     int count,
                                     const int *functions,
                                     const double *lower,
                                     const double *upper,
                                     int *first);
BIYOS_API BIYOS_status BIYOS_setconbounds(BIYOS_model *model,
                                          int count,
                                          const int *indices,
                                          const double *lower,
                                          const double *upper);
BIYOS_API BIYOS_status BIYOS_getncons(BIYOS_model *model, int *count);
BIYOS_API BIYOS_status BIYOS_getcons(BIYOS_model *model,
                                     int first,
                                     int count,
                                     int *functions,
                                     double *lower,
                                     double *upper);
BIYOS_API BIYOS_status BIYOS_addobjs(BIYOS_model *model,
                                     int count,
                                     const int *functions,
                                     const int *senses,
                                     const int *priorities,
                                     const double *weights,
                                     const double *absolute,
                                     const double *relative,
                                     int *first);
BIYOS_API BIYOS_status BIYOS_setobjs(BIYOS_model *model,
                                     int count,
                                     const int *indices,
                                     const int *senses,
                                     const int *priorities,
                                     const double *weights,
                                     const double *absolute,
                                     const double *relative);
BIYOS_API BIYOS_status BIYOS_getnobjs(BIYOS_model *model, int *count);
BIYOS_API BIYOS_status BIYOS_getobjs(BIYOS_model *model,
                                     int first,
                                     int count,
                                     int *functions,
                                     int *senses,
                                     int *priorities,
                                     double *weights,
                                     double *absolute,
                                     double *relative);

#ifdef __cplusplus
}
#endif
