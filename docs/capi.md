# C API

`biyos/biyos.h` is the binary interface of biyos. It is C99 and includes the other headers of `include/biyos/`, such as `biyos/options.h` for the option ids. Every function, type and macro in them has an entry below.

## Conventions

Every call that returns `biyos_status` follows these rules.

- It returns `BIYOS_OK` or a negative error code.
- On failure it keeps its message on the environment, read with `biyos_last_error_message`. It keeps none when given a null environment, and `biyos_env_create` keeps none since it has no environment yet. If memory runs out while the message is kept, the message is left empty.
- On success it leaves the message as it was.
- A null pointer argument, the environment included, gives `BIYOS_INVALID_ARGUMENT`, unless its entry says otherwise.
- On failure it leaves its output arguments as they were, unless its entry says otherwise.
- Besides the codes its entry names, it can return `BIYOS_NO_MEMORY` when an allocation fails and `BIYOS_INTERNAL_ERROR` on a fault in biyos.

No call lets an exception out. A call keeps no pointer the caller passes in, unless its entry says so. A string a call returns belongs to biyos, and its entry says how long the pointer stays valid.

Every number the headers define, such as an option id, an option kind or an error code, keeps its value for good and is never reused. A new one takes the next free number of its group. An option that is no longer wanted is retired, not removed: it keeps its id, and setting it reports that it is retired.

## Types

| Type | What |
| --- | --- |
| `biyos_status` | an `int`, `BIYOS_OK` or an error code |
| `biyos_env` | an opaque environment, holding the options, the subscriptions and the last error message |
| `biyos_event` | an opaque event, read with the `biyos_event_*` calls while a subscriber runs |
| `biyos_subscriber` | `void (*)(const biyos_event *event, void *user_data)`, a function called with each event a subscription receives |

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
| `BIYOS_INVALID_ARGUMENT` | -100 | a null pointer or environment, an unknown option name or id, a call of the wrong kind for its option, a value the option does not accept, an unknown event kind, level or subscription, or a change to an environment while it delivers an event |
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
biyos_status biyos_env_free(biyos_env *env);
```

Frees the environment and every string read from it. A null `env` gives `BIYOS_OK`. While `env` delivers an event it returns `BIYOS_INVALID_ARGUMENT` and frees nothing, as every call that would change `env` does then (see Subscribers).

### `biyos_last_error_message`

```c
const char *biyos_last_error_message(const biyos_env *env);
```

Returns the message of the last call on `env` that failed, or an empty string if none has. Successful calls leave it in place. A call that fails inside a subscriber is the last failure only until that subscriber returns: the message is then again what it was before the subscriber ran. It starts with the kind of failure (`invalid argument`, `out of memory` or `internal error`) and is written for people: branch on the status code, not on the text. It is also empty after a failure if memory ran out while keeping the message.

The pointer is never null. It stays valid until the next failed call on `env` or until `env` is freed, and one read inside a subscriber only until that subscriber returns. A null `env` gives an empty string that is always valid.

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
| `BIYOS_OPT_FEASIBILITY_ABS` | 6 | `feasibility_abs` | double | 1e-6 | [0, infinity] | Absolute tolerance of a bound or an equality. |
| `BIYOS_OPT_FEASIBILITY_REL` | 7 | `feasibility_rel` | double | 1e-6 | [0, infinity] | Relative tolerance of a bound or an equality. |
| `BIYOS_OPT_INTEGRALITY_TOL` | 8 | `integrality_tol` | double | 1e-5 | [0, infinity] | Largest distance to the nearest integer of a value taken as integral. |
| `BIYOS_OPT_ZERO_TOL` | 9 | `zero_tol` | double | 1e-9 | [0, infinity] | Largest magnitude of a value taken as zero. |
| `BIYOS_OPT_CPU_TIME_LIMIT` | 10 | `cpu_time_limit` | double | infinity | [0, infinity] | CPU time limit in seconds, counting every thread of the process. |

The ranges in the Accepts column include their ends and are checked exactly, with no tolerance, and NaN is outside every range. Nothing acts on these options yet, since biyos solves nothing: they are checked, stored and read back.

The tolerance options say when a value of a model, such as a variable or a constraint function at a point, meets a lower or upper bound the model puts on it, equals another such value, is zero or is integral. A finite value meets a finite bound unless it lies beyond it by more than both `feasibility_abs` and `feasibility_rel` times the larger of 1 and the magnitude of the bound. Infinities take no tolerance: when the value or the bound is infinite, the value meets a lower bound exactly when it is at least the bound, and an upper bound exactly when it is at most the bound. Two finite values are equal unless they differ by more than both `feasibility_abs` and `feasibility_rel` times the larger of 1 and their magnitudes. An infinite value equals only itself and is not integral. NaN is not zero, not integral, equal to no value and meets no bound.

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

biyos never prints. It reports through events, and a caller reads them by subscribing a function to one kind of event at a lowest level. The subscription then receives every event of that kind at that level or a more severe one. The subscriptions of an environment receive an event in the order they were made, and an event nobody wants is not built.

| Macro | Value | Kind |
| --- | --- | --- |
| `BIYOS_EVENT_MESSAGE` | 1 | a message: a level, the module that sent it, a text and fields |
| `BIYOS_EVENT_CHECK` | 2 | a failed check: the invariant that does not hold, the object it was checked on and the values involved |

| Macro | Value | Level |
| --- | --- | --- |
| `BIYOS_LEVEL_DEBUG` | 1 | debug, the least severe |
| `BIYOS_LEVEL_INFO` | 2 | info |
| `BIYOS_LEVEL_WARNING` | 3 | warning |
| `BIYOS_LEVEL_ERROR` | 4 | error, the most severe |

A level is more severe the larger its value. The kinds and levels come from `biyos/events.h`.

A field is a name and a value, both text. The only events biyos sends so far are error messages. A call on an environment that fails sends one, unless the environment is delivering an event: its level is `BIYOS_LEVEL_ERROR`, its module `capi`, its text the message `biyos_last_error_message` returns for the failure, and its one field `code` the returned status as decimal text, such as `-100`, so a subscriber can tell the group from the range.

### Subscribers

A subscriber is called with the event and the `user_data` its subscription was given. While it runs, the environment delivers an event and is read-only.

- The subscriber may read options, the last error message and the event.
- Every call that would change the environment, setting an option, subscribing, unsubscribing or freeing it, fails with `BIYOS_INVALID_ARGUMENT` and the message `invalid argument: environment: is read-only while delivering an event`, and changes nothing.
- A call that fails inside a subscriber keeps its message as usual, so `biyos_last_error_message` explains it, but sends no event.
- Inside an error event, `biyos_last_error_message` returns that event's failure. When the subscriber returns, the message is again what it was before the subscriber ran, so one subscriber's failures change neither what the next subscriber reads nor what the caller reads after the call.
- The event and every string read from it are valid only until the subscriber returns.

### `biyos_subscribe`

```c
biyos_status biyos_subscribe(biyos_env *env, biyos_subscriber subscriber, void *user_data,
                             int kind, int lowest, int *subscription);
```

Subscribes `subscriber` to the events of `kind`, a `BIYOS_EVENT_*` value, at level `lowest`, a `BIYOS_LEVEL_*` value, and above, and writes the id of the new subscription to `*subscription`. Ids count from 1 in each environment and are never reused. biyos keeps `subscriber` and `user_data` until the subscription is removed with `biyos_unsubscribe` or `env` is freed, and passes `user_data` back as given: it may be null. Returns `BIYOS_INVALID_ARGUMENT` for an unknown kind or level. A failed call subscribes nothing. An environment that has given out 2147483646 ids returns `BIYOS_NO_MEMORY`.

### `biyos_unsubscribe`

```c
biyos_status biyos_unsubscribe(biyos_env *env, int subscription);
```

Removes the subscription with id `subscription`, whose subscriber is not called again. Returns `BIYOS_INVALID_ARGUMENT` when `env` has no such subscription, as for one already removed.

### Reading an event

These calls read the event a subscriber is given. They cannot fail. A null `event` gives 0 or an empty string, as does a field number outside the event's fields. A string they return is valid until the subscriber returns.

### `biyos_event_kind`

```c
int biyos_event_kind(const biyos_event *event);
```

Returns the kind of the event, a `BIYOS_EVENT_*` value.

### `biyos_event_level`

```c
int biyos_event_level(const biyos_event *event);
```

Returns the level of the event, a `BIYOS_LEVEL_*` value.

### `biyos_event_module`

```c
const char *biyos_event_module(const biyos_event *event);
```

Returns the name of the module that sent the event, such as `capi`.

### `biyos_event_text`

```c
const char *biyos_event_text(const biyos_event *event);
```

Returns the text of the event.

### `biyos_event_field_count`

```c
int biyos_event_field_count(const biyos_event *event);
```

Returns the number of fields of the event. Fields are numbered from 0.

### `biyos_event_field_name`

```c
const char *biyos_event_field_name(const biyos_event *event, int field);
```

Returns the name of field number `field`.

### `biyos_event_field_value`

```c
const char *biyos_event_field_value(const biyos_event *event, int field);
```

Returns the value of field number `field`.

## Solve status

`biyos/status.h` holds the numbers that describe how a solve ended. The termination says why the solve stopped, a result status says what a returned point is, the primal point and the dual point each having one, and the claim says what was proved. They are kept apart, so a result never says more than its method proved. None of them is an error code: a limit reached or an infeasible model is an outcome, not a failed call. No call returns them yet, since biyos solves nothing.

| Macro | Value | Meaning |
| --- | --- | --- |
| `BIYOS_TERMINATION_NOT_SOLVED` | 0 | no solve has ended |
| `BIYOS_TERMINATION_OPTIMAL` | 101 | a global optimum was found and proved. It comes only with the claim `BIYOS_CLAIM_GLOBAL` |
| `BIYOS_TERMINATION_LOCALLY_SOLVED` | 102 | a local solution: every convergence test held at its tolerance |
| `BIYOS_TERMINATION_ALMOST_LOCALLY_SOLVED` | 103 | every convergence test held at its relaxed tolerance, after the strict tolerances were not all met |
| `BIYOS_TERMINATION_ITERATION_LIMIT` | 201 | the iteration limit was reached |
| `BIYOS_TERMINATION_TIME_LIMIT` | 202 | the wall time limit was reached |
| `BIYOS_TERMINATION_CPU_TIME_LIMIT` | 203 | the CPU time limit was reached |
| `BIYOS_TERMINATION_EVALUATION_LIMIT` | 204 | the limit on function evaluations was reached |
| `BIYOS_TERMINATION_USER_STOP` | 205 | the user asked the solve to stop |
| `BIYOS_TERMINATION_INFEASIBLE` | 301 | the model was proved infeasible. The dual result status says whether a certificate comes with it: `BIYOS_RESULT_CERTIFICATE` when one does |
| `BIYOS_TERMINATION_LOCALLY_INFEASIBLE` | 302 | the solve reached a local minimum of the infeasibility that is not zero. Nothing proves the model infeasible |
| `BIYOS_TERMINATION_UNBOUNDED` | 303 | the objective is unbounded below on the feasible set |
| `BIYOS_TERMINATION_SLOW_PROGRESS` | 401 | progress stalled before the convergence tests held |
| `BIYOS_TERMINATION_NUMERICAL_TROUBLE` | 402 | numerical difficulties kept the solve from going on |

Terminations are grouped by hundreds, like the error codes. A new one joins its group, so test a group by range rather than by value.

| Codes | Group |
| --- | --- |
| 100 to 199 | solved |
| 200 to 299 | stopped early, by a limit or by the user |
| 300 to 399 | infeasibility or unboundedness detected |
| 400 to 499 | no progress |

A result status describes the primal point or the dual point. For the dual point, feasible means feasible for the dual problem.

| Macro | Value | Meaning |
| --- | --- | --- |
| `BIYOS_RESULT_NONE` | 0 | no point is returned |
| `BIYOS_RESULT_FEASIBLE` | 1 | the point is feasible |
| `BIYOS_RESULT_NEARLY_FEASIBLE` | 2 | the point is feasible only at the relaxed tolerance |
| `BIYOS_RESULT_INFEASIBLE` | 3 | the point is infeasible |
| `BIYOS_RESULT_CERTIFICATE` | 4 | the point is a certificate of infeasibility: a primal one proves the dual problem infeasible, a dual one the primal problem |
| `BIYOS_RESULT_NEARLY_CERTIFICATE` | 5 | the point is a certificate only at the relaxed tolerance |
| `BIYOS_RESULT_UNKNOWN` | 6 | a point is returned, and nothing is known of it |

The claim says how far the termination was proved to hold.

| Macro | Value | Meaning |
| --- | --- | --- |
| `BIYOS_CLAIM_NONE` | 0 | nothing is proved |
| `BIYOS_CLAIM_LOCAL` | 1 | what the termination says holds near the returned point |
| `BIYOS_CLAIM_GLOBAL` | 2 | what the termination says holds for the whole model |
