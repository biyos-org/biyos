# C API

`biyos/biyos.h` is the binary interface of biyos. It is C99 and includes the other headers of `include/biyos/`, such as `biyos/options.h` for the option ids. Every function, type and macro in them has an entry below.

## Conventions

Every call that returns `biyos_status` follows these rules.

- It returns `BIYOS_OK` or a negative error code.
- On failure it keeps its message on the environment, read with `biyos_last_error_message`. It keeps none when given a null environment, and `biyos_env_create` keeps none since it has no environment yet. If memory runs out while the message is kept, the message is left empty.
- On success it leaves the message as it was.
- A null pointer argument, the environment included, gives `BIYOS_INVALID_ARGUMENT`.
- On failure it leaves its output arguments as they were, unless its entry says otherwise.
- Besides the codes its entry names, it can return `BIYOS_NO_MEMORY` when an allocation fails and `BIYOS_INTERNAL_ERROR` on a fault in biyos.

No call lets an exception out. A call keeps no pointer the caller passes in, unless its entry says so. A string a call returns belongs to biyos, and its entry says how long the pointer stays valid.

Every number the headers define, such as an option id, an option kind or an error code, keeps its value for good and is never reused. A new one takes the next free number of its group. An option that is no longer wanted is retired, not removed: it keeps its id, and setting it reports that it is retired.

## Types

| Type | What |
| --- | --- |
| `biyos_status` | an `int`, `BIYOS_OK` or an error code |
| `biyos_env` | an opaque environment, holding the options and the last error message |

## Linking

| Macro | Use |
| --- | --- |
| `BIYOS_API` | marks every exported function. It is `__declspec(dllexport)` while the library builds on Windows, `__declspec(dllimport)` for its callers there, `__attribute__((visibility("default")))` elsewhere, and empty under `BIYOS_STATIC` |
| `BIYOS_STATIC` | defined by a caller that links the static library, so that on Windows the functions are not imported from a DLL. When biyos is built with `-DBUILD_SHARED_LIBS=OFF`, a CMake target that links `biyos::biyos` gets it from the library |
| `BIYOS_BUILDING` | defined by the library's own build. A caller never defines it |

## Status codes

| Macro | Value | Meaning |
| --- | --- | --- |
| `BIYOS_OK` | 0 | the call succeeded |
| `BIYOS_INVALID_ARGUMENT` | -100 | a null pointer or environment, an unknown option name or id, a call of the wrong kind for its option, or a value the option does not accept |
| `BIYOS_NO_MEMORY` | -400 | an allocation failed |
| `BIYOS_INTERNAL_ERROR` | -500 | a fault in biyos |

The hundreds say who is at fault. A new code joins its group, so test a group by range rather than by value. No call returns a positive status.

| Codes | At fault |
| --- | --- |
| -100 to -199 | the caller |
| -200 to -299 | the model |
| -300 to -399 | user code, such as a callback |
| -400 to -499 | resources |
| -500 to -599 | biyos |

No code exists yet in the model and user code groups.

## Version

### `biyos_version`

```c
void biyos_version(int *major, int *minor, int *patch);
```

Writes the version of the library. A null argument is skipped. It cannot fail.

## Environment

Environments share no state, so separate threads may use separate environments at the same time. One environment is used by one thread at a time.

### `biyos_env_create`

```c
biyos_status biyos_env_create(biyos_env **env);
```

Creates an environment with every option at its default and an empty error message, and writes it to `*env`. On failure it sets `*env` to null. Returns `BIYOS_INVALID_ARGUMENT` when `env` is null.

### `biyos_env_free`

```c
void biyos_env_free(biyos_env *env);
```

Frees the environment and every string read from it. A null `env` is ignored.

### `biyos_last_error_message`

```c
const char *biyos_last_error_message(const biyos_env *env);
```

Returns the message of the last call on `env` that failed, or an empty string if none has. Successful calls leave it in place. It starts with the kind of failure (`invalid argument`, `out of memory` or `internal error`) and is written for people: branch on the status code, not on the text. It is also empty after a failure if memory ran out while keeping the message.

The pointer is never null. It stays valid until the next failed call on `env` or until `env` is freed. A null `env` gives an empty string that is always valid.

## Options

An option has a name, an id and a kind. Its kind decides which calls set and get it, and a call of another kind returns `BIYOS_INVALID_ARGUMENT`. Nothing is converted, so an int option rejects `biyos_set_double_option`. Every call that takes an option id returns `BIYOS_INVALID_ARGUMENT` when it is not a known id. A failed set leaves the option as it was.

| Macro | Value | Kind | Set and get with |
| --- | --- | --- | --- |
| `BIYOS_KIND_BOOL` | 1 | bool | `biyos_set_bool_option`, `biyos_get_bool_option` |
| `BIYOS_KIND_INT` | 2 | int | `biyos_set_int_option`, `biyos_get_int_option` |
| `BIYOS_KIND_DOUBLE` | 3 | double | `biyos_set_double_option`, `biyos_get_double_option` |
| `BIYOS_KIND_STRING` | 4 | string | `biyos_set_string_option`, `biyos_get_string_option` |
| `BIYOS_KIND_ENUM` | 5 | enum | `biyos_set_string_option`, `biyos_get_string_option`, by the name of the value |

The ids are the `BIYOS_OPT_*` macros of `biyos/options.h` and count from 1. `biyos_option_id` finds the id of a name at run time, and `biyos_option_kind` the kind of an id.

| Macro | Value | Name | Kind | Default | Accepts | Meaning |
| --- | --- | --- | --- | --- | --- | --- |
| `BIYOS_OPT_SEED` | 1 | `seed` | int | 0 | [0, 2147483647] | Seed of every random choice. |
| `BIYOS_OPT_TIME_LIMIT` | 2 | `time_limit` | double | infinity | [0, infinity] | Wall time limit in seconds. |
| `BIYOS_OPT_CHECK_LEVEL` | 3 | `check_level` | enum | `cheap` | `none`, `cheap`, `full` | Invariant checks to run. |
| `BIYOS_OPT_PRESOLVE` | 4 | `presolve` | bool | true | true, false | Simplify the model before solving it. |
| `BIYOS_OPT_REPRODUCER` | 5 | `reproducer` | string | empty | any string | File the solve writes its model, options, seed and version to. Empty for none. |

Ranges include their bounds, and NaN is outside every range. Nothing acts on these options yet, since biyos solves nothing: they are checked, stored and read back.

### `biyos_option_id`

```c
biyos_status biyos_option_id(biyos_env *env, const char *name, int *option);
```

Writes to `*option` the id of the option called `name`, which must match exactly. Returns `BIYOS_INVALID_ARGUMENT` when no option has that name.

### `biyos_option_kind`

```c
biyos_status biyos_option_kind(biyos_env *env, int option, int *kind);
```

Writes to `*kind` the kind of the option, one of the `BIYOS_KIND_*` values.

### `biyos_set_bool_option`

```c
biyos_status biyos_set_bool_option(biyos_env *env, int option, int value);
```

Sets a bool option to true when `value` is nonzero and to false when it is zero.

### `biyos_set_int_option`

```c
biyos_status biyos_set_int_option(biyos_env *env, int option, int value);
```

Sets an int option. Returns `BIYOS_INVALID_ARGUMENT` when `value` is outside its range.

### `biyos_set_double_option`

```c
biyos_status biyos_set_double_option(biyos_env *env, int option, double value);
```

Sets a double option. Returns `BIYOS_INVALID_ARGUMENT` when `value` is outside its range or is NaN.

### `biyos_set_string_option`

```c
biyos_status biyos_set_string_option(biyos_env *env, int option, const char *value);
```

Sets a string or enum option to a copy of `value`. For an enum option, `value` is the name of one of its values and must match exactly. Returns `BIYOS_INVALID_ARGUMENT` when it names none of them. On success, pointers read earlier from this option are no longer valid.

### `biyos_get_bool_option`

```c
biyos_status biyos_get_bool_option(biyos_env *env, int option, int *value);
```

Writes 1 to `*value` when the bool option is true and 0 when it is false.

### `biyos_get_int_option`

```c
biyos_status biyos_get_int_option(biyos_env *env, int option, int *value);
```

Writes the value of an int option to `*value`.

### `biyos_get_double_option`

```c
biyos_status biyos_get_double_option(biyos_env *env, int option, double *value);
```

Writes the value of a double option to `*value`.

### `biyos_get_string_option`

```c
biyos_status biyos_get_string_option(biyos_env *env, int option, const char **value);
```

Writes to `*value` the value of a string option, or the name of the value of an enum option. The pointer stays valid until the option is set again or `env` is freed. A failed set of the option and a set of another option leave it valid.

## Events

| Macro | Value | Kind |
| --- | --- | --- |
| `BIYOS_EVENT_MESSAGE` | 1 | a message: a level, the module that sent it, a text and fields |

| Macro | Value | Level |
| --- | --- | --- |
| `BIYOS_LEVEL_DEBUG` | 1 | debug, the least severe |
| `BIYOS_LEVEL_INFO` | 2 | info |
| `BIYOS_LEVEL_WARNING` | 3 | warning |
| `BIYOS_LEVEL_ERROR` | 4 | error, the most severe |

A level is more severe the larger its value. The kinds and levels come from `biyos/events.h`.
