#pragma once

#include "biyos/events.h"
#include "biyos/options.h"
#include "biyos/status.h"

#include <stddef.h>

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
BIYOS_API biyos_status biyos_option_versions(biyos_env *env, int option, int *since,
                                             int *deprecated, int *retired);
BIYOS_API biyos_status biyos_set_bool_option(biyos_env *env, int option, int value);
BIYOS_API biyos_status biyos_set_int_option(biyos_env *env, int option, int value);
BIYOS_API biyos_status biyos_set_double_option(biyos_env *env, int option, double value);
BIYOS_API biyos_status biyos_set_string_option(biyos_env *env, int option, const char *value);
BIYOS_API biyos_status biyos_get_bool_option(biyos_env *env, int option, int *value);
BIYOS_API biyos_status biyos_get_int_option(biyos_env *env, int option, int *value);
BIYOS_API biyos_status biyos_get_double_option(biyos_env *env, int option, double *value);
BIYOS_API biyos_status biyos_get_string_option(biyos_env *env, int option, const char **value);

BIYOS_API biyos_status biyos_subscribe(biyos_env *env, biyos_subscriber subscriber, void *user_data,
                                       int kind, int lowest, int *subscription);
BIYOS_API biyos_status biyos_unsubscribe(biyos_env *env, int subscription);

BIYOS_API int biyos_event_kind(const biyos_event *event);
BIYOS_API int biyos_event_level(const biyos_event *event);
BIYOS_API const char *biyos_event_module(const biyos_event *event);
BIYOS_API const char *biyos_event_text(const biyos_event *event);
BIYOS_API int biyos_event_field_count(const biyos_event *event);
BIYOS_API const char *biyos_event_field_name(const biyos_event *event, int field);
BIYOS_API const char *biyos_event_field_value(const biyos_event *event, int field);

BIYOS_API biyos_status biyos_format_text(const biyos_event *event, char *buffer, size_t size,
                                         size_t *needed);
BIYOS_API biyos_status biyos_format_json(const biyos_event *event, char *buffer, size_t size,
                                         size_t *needed);

#ifdef __cplusplus
}
#endif
