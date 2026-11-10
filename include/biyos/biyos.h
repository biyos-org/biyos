#pragma once

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

#define BIYOS_OK 0
#define BIYOS_INVALID_ARGUMENT (-100)
#define BIYOS_NO_MEMORY (-400)
#define BIYOS_INTERNAL_ERROR (-500)

BIYOS_API void biyos_version(int *major, int *minor, int *patch);

BIYOS_API biyos_status biyos_env_create(biyos_env **env);
BIYOS_API void biyos_env_free(biyos_env *env);
BIYOS_API const char *biyos_last_error_message(const biyos_env *env);

#ifdef __cplusplus
}
#endif
