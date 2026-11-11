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

typedef int biyos_status;
typedef struct biyos_env biyos_env;
typedef struct biyos_event biyos_event;
typedef struct biyos_model biyos_model;
typedef void (*biyos_subscriber)(const biyos_event *event, void *user_data);

#define BIYOS_OK 0
#define BIYOS_INVALID_ARGUMENT (-100)
#define BIYOS_NO_MEMORY (-400)
#define BIYOS_INTERNAL_ERROR (-500)

BIYOS_API void biyos_version(int *major, int *minor, int *patch);

BIYOS_API biyos_status biyos_env_create(biyos_env **env);
BIYOS_API biyos_status biyos_env_free(biyos_env *env);
BIYOS_API const char *biyos_last_error_message(const biyos_env *env);

BIYOS_API biyos_status biyos_option_id(biyos_env *env, const char *name, int *option);
BIYOS_API biyos_status biyos_option_kind(biyos_env *env, int option, int *kind);
BIYOS_API biyos_status biyos_option_versions(biyos_env *env,
                                             int option,
                                             int *since,
                                             int *deprecated,
                                             int *retired);
BIYOS_API biyos_status biyos_set_bool_option(biyos_env *env, int option, int value);
BIYOS_API biyos_status biyos_set_int_option(biyos_env *env, int option, int value);
BIYOS_API biyos_status biyos_set_double_option(biyos_env *env, int option, double value);
BIYOS_API biyos_status biyos_set_string_option(biyos_env *env, int option, const char *value);
BIYOS_API biyos_status biyos_get_bool_option(biyos_env *env, int option, int *value);
BIYOS_API biyos_status biyos_get_int_option(biyos_env *env, int option, int *value);
BIYOS_API biyos_status biyos_get_double_option(biyos_env *env, int option, double *value);
BIYOS_API biyos_status biyos_get_string_option(biyos_env *env, int option, const char **value);

BIYOS_API biyos_status biyos_subscribe(biyos_env *env,
                                       biyos_subscriber subscriber,
                                       void *user_data,
                                       int kind,
                                       int lowest,
                                       int *subscription);
BIYOS_API biyos_status biyos_unsubscribe(biyos_env *env, int subscription);

BIYOS_API int biyos_event_kind(const biyos_event *event);
BIYOS_API int biyos_event_level(const biyos_event *event);
BIYOS_API const char *biyos_event_module(const biyos_event *event);
BIYOS_API const char *biyos_event_text(const biyos_event *event);
BIYOS_API int biyos_event_field_count(const biyos_event *event);
BIYOS_API const char *biyos_event_field_name(const biyos_event *event, int field);
BIYOS_API const char *biyos_event_field_value(const biyos_event *event, int field);

BIYOS_API biyos_status biyos_format_text(const biyos_event *event,
                                         char *buffer,
                                         size_t size,
                                         size_t *needed);
BIYOS_API biyos_status biyos_format_json(const biyos_event *event,
                                         char *buffer,
                                         size_t size,
                                         size_t *needed);

BIYOS_API biyos_status biyos_model_create(biyos_env *env, biyos_model **model);
BIYOS_API biyos_status biyos_model_copy(biyos_model *model, biyos_env *env, biyos_model **copy);
BIYOS_API biyos_status biyos_model_free(biyos_model *model);
BIYOS_API biyos_status biyos_add_variables(biyos_model *model,
                                           int count,
                                           const int *kinds,
                                           const double *lower,
                                           const double *upper,
                                           int *first);
BIYOS_API biyos_status biyos_set_variable_bounds(biyos_model *model,
                                                 int count,
                                                 const int *indices,
                                                 const double *lower,
                                                 const double *upper);
BIYOS_API biyos_status biyos_set_variable_kinds(biyos_model *model,
                                                int count,
                                                const int *indices,
                                                const int *kinds);
BIYOS_API biyos_status biyos_variable_count(biyos_model *model, int *count);
BIYOS_API biyos_status biyos_get_variables(biyos_model *model,
                                           int first,
                                           int count,
                                           int *kinds,
                                           double *lower,
                                           double *upper);
BIYOS_API biyos_status biyos_add_functions(biyos_model *model,
                                           int count,
                                           const double *constants,
                                           const int64_t *starts,
                                           const int *variables,
                                           const double *values,
                                           int *first);
BIYOS_API biyos_status biyos_function_count(biyos_model *model, int *count);
BIYOS_API biyos_status biyos_get_functions(biyos_model *model,
                                           int first,
                                           int count,
                                           double *constants,
                                           int64_t *starts,
                                           int64_t size,
                                           int64_t *needed,
                                           int *variables,
                                           double *values);
BIYOS_API biyos_status biyos_add_quadratic_terms(biyos_model *model,
                                                 int count,
                                                 const int *functions,
                                                 const int64_t *starts,
                                                 const int *firsts,
                                                 const int *seconds,
                                                 const double *values);
BIYOS_API biyos_status biyos_get_quadratic_terms(biyos_model *model,
                                                 int first,
                                                 int count,
                                                 int64_t *starts,
                                                 int64_t size,
                                                 int64_t *needed,
                                                 int *firsts,
                                                 int *seconds,
                                                 double *values);
BIYOS_API biyos_status biyos_add_constraints(biyos_model *model,
                                             int count,
                                             const int *functions,
                                             const double *lower,
                                             const double *upper,
                                             int *first);
BIYOS_API biyos_status biyos_set_constraint_bounds(biyos_model *model,
                                                   int count,
                                                   const int *indices,
                                                   const double *lower,
                                                   const double *upper);
BIYOS_API biyos_status biyos_constraint_count(biyos_model *model, int *count);
BIYOS_API biyos_status biyos_get_constraints(biyos_model *model,
                                             int first,
                                             int count,
                                             int *functions,
                                             double *lower,
                                             double *upper);

#ifdef __cplusplus
}
#endif
