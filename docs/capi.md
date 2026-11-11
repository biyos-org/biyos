# C API

`biyos/biyos.h` is the binary interface of biyos. It is C99 and includes the other headers of `include/biyos/`, such as `biyos/options.h` for the option ids. Every function, type and macro in them has an entry below.

## Conventions

Every call that returns `biyos_status` follows these rules.

- It returns `BIYOS_OK` or a negative error code.
- On failure it keeps its message on the environment, read with `biyos_last_error_message`, which for a call on a model is the environment the model was made on. It keeps none when given a null environment or model or when it takes none, as the formatting calls do, and `biyos_env_create` keeps none since it has no environment yet. If memory runs out while the message is kept, the message is left empty.
- On success it leaves the message as it was.
- A null pointer argument, the environment or the model included, gives `BIYOS_INVALID_ARGUMENT`, unless its entry says otherwise.
- On failure it leaves its output arguments as they were, unless its entry says otherwise.
- Besides the codes its entry names, it can return `BIYOS_NO_MEMORY` when an allocation fails and `BIYOS_INTERNAL_ERROR` on a fault in biyos.

No call lets an exception out. A call keeps no pointer the caller passes in, unless its entry says so. A string a call returns belongs to biyos, and its entry says how long the pointer stays valid.

Every number the headers define, such as an option id, an option kind or an error code, keeps its value for good and is never reused. A new one takes the next free number of its group. An option that is no longer wanted is retired, not removed: it keeps its id, and setting it reports that it is retired.

## Types

| Type | What |
| --- | --- |
| `biyos_status` | an `int`, `BIYOS_OK` or an error code |
| `biyos_env` | an opaque environment, holding the options, the subscriptions and the last error message |
| `biyos_event` | an opaque event, read with the `biyos_event_*` calls and formatted with `biyos_format_text` and `biyos_format_json` while a subscriber runs |
| `biyos_model` | an opaque model, made on an environment and holding the variables, functions, constraints and objectives of a problem |
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
| `BIYOS_INVALID_ARGUMENT` | -100 | a null pointer, environment or model, an unknown option name or id, a call of the wrong kind for its option, a value the option does not accept, a set of a retired option, an unknown event kind, level or subscription, a negative count or size, an unknown variable kind or variable index, a bound a variable does not accept, an unknown function index, starts that do not begin at 0 or that decrease, a term whose variable is not one of the model or appears twice in a function, a constant or a coefficient that is not finite, an unknown constraint index, a constraint on a function the model does not hold, a bound a constraint does not accept, an unknown objective index, an objective on a function the model does not hold, an unknown sense, a weight that is not finite and positive, a tolerance that is NaN or negative, a change to an environment or to a model made on it while the environment delivers an event, or a free of an environment that has a model not yet freed |
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

Environments share no state, so separate threads may use separate environments at the same time. One environment is used by one thread at a time, together with the models made on it.

### `biyos_env_create`

```c
biyos_status biyos_env_create(biyos_env **env);
```

Creates an environment with every option at its default and an empty error message, and writes it to `*env`. On failure it sets `*env` to null. Returns `BIYOS_INVALID_ARGUMENT` when `env` is null.

### `biyos_env_free`

```c
biyos_status biyos_env_free(biyos_env *env);
```

Frees the environment and every string read from it. A null `env` gives `BIYOS_OK`. While `env` delivers an event it returns `BIYOS_INVALID_ARGUMENT` and frees nothing, as every call that would change `env` does then (see Subscribers). While a model made on `env` is not freed, it returns `BIYOS_INVALID_ARGUMENT` with the message `invalid argument: environment: has a model that is not freed`, and frees nothing.

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

Each option records the version of biyos that added it and, once it is no longer wanted, the version that deprecated it and the version that retired it, read with `biyos_option_versions`. A deprecated option still works: setting it sets it and sends a message event at level `BIYOS_LEVEL_WARNING`, from module `core`, with a text such as `option 'old': deprecated in 0.2.0, use 'new'` and the fields `option`, `deprecated` and `replacement`, in that order: the name of the option, the version that deprecated it and the name of the option to use instead. A failed set sends no warning. Setting a retired option, by any call, returns `BIYOS_INVALID_ARGUMENT` with a message such as `invalid argument: option 'old': retired in 0.3.0, use 'new'` and changes nothing, and getting it still gives its value. An option is deprecated before it is retired, and the option to use instead is neither. Every option above was added in 0.1.0, and none is deprecated or retired.

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

### `biyos_option_versions`

```c
biyos_status biyos_option_versions(biyos_env *env, int option, int *since, int *deprecated,
                                   int *retired);
```

Writes three versions of biyos: to `*since` the one that added the option, to `*deprecated` the one that deprecated it and to `*retired` the one that retired it. Each is written as `10000 * major + 100 * minor + patch`, such as 100 for 0.1.0, or as 0 when the option is not deprecated or not retired. A minor or patch number stays below 100, so two versions compare as their numbers do.

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
| `BIYOS_EVENT_PHASE` | 3 | a phase of a solve beginning or ending: its name and the wall and CPU time |

| Macro | Value | Level |
| --- | --- | --- |
| `BIYOS_LEVEL_DEBUG` | 1 | debug, the least severe |
| `BIYOS_LEVEL_INFO` | 2 | info |
| `BIYOS_LEVEL_WARNING` | 3 | warning |
| `BIYOS_LEVEL_ERROR` | 4 | error, the most severe |

A level is more severe the larger its value. The kinds and levels come from `biyos/events.h`.

A field is a name and a value, both text. Messages, check events and phase events are described below in turn. The messages biyos sends so far are an error when a call fails and a warning when a deprecated option is set. A call on an environment that fails sends an error message, unless the environment is delivering an event: its level is `BIYOS_LEVEL_ERROR`, its module `capi`, its text the message `biyos_last_error_message` returns for the failure, and its one field `code` the returned status as decimal text, such as `-100`, so a subscriber can tell the group from the range. Setting a deprecated option sends a warning message from module `core`, whose text and fields are given under Options. No option is deprecated yet.

A check event reports an invariant that does not hold, found by a check the `check_level` option runs, and the call then fails. Its level is `BIYOS_LEVEL_ERROR`, its module the module that ran the check, its text the message `biyos_last_error_message` returns for the failure, and its fields are `invariant`, `object` and `values`, in that order. It is not sent while the environment is delivering an event. A check that fails while biyos is building an event sends no check event and does not fail the call: the event being built is not sent, the check's error is dropped, and the last error message is left as it was. No call runs a check yet.

A phase event marks a phase of a solve, such as presolve, beginning or ending. Phases nest: a phase begun inside another ends before it. Its level is `BIYOS_LEVEL_INFO`, its module the module that timed the phase, its text the name of the phase, and its fields are `edge`, `wall` and `cpu`, in that order. `edge` is `begin` or `end`. `wall` is the wall time since the environment was created and `cpu` the CPU time the process has used, every thread included, both read as the phase begins or ends and written in nanoseconds as decimal text. biyos adds up the time of each phase in a timing tree, nested as phases nest, which no call reads yet. The tree leaves out the time spent delivering phase events, so subscribing never changes it, but the stamps do not: a phase's time in the tree is its end's `wall` and `cpu` less its begin's, less, for each phase event sent between the two, the time from that event's stamps until its subscribers return. Those events are the phase's own begin event and the events of the phases inside it. Should the CPU time be unreadable as a phase begins, the call fails with `BIYOS_INTERNAL_ERROR`, and the phase neither begins nor sends an event. Should it be unreadable as a phase ends, `cpu` repeats its begin value and the phase counts as using no CPU time. No call times a phase yet.

### Subscribers

A subscriber is called with the event and the `user_data` its subscription was given. While it runs, the environment delivers an event and is read-only.

- The subscriber may read options, the last error message, the event and the models made on the environment.
- Every call that would change the environment, setting an option, subscribing, unsubscribing, creating a model on it or freeing it, fails with `BIYOS_INVALID_ARGUMENT` and the message `invalid argument: environment: is read-only while delivering an event`, and changes nothing.
- Every call that would change or free a model made on the environment fails with `BIYOS_INVALID_ARGUMENT` and the message `invalid argument: model: is read-only while delivering an event`, and changes nothing.
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

### Formatting an event

These calls write an event as one line into a buffer the caller gives, in the manner of `snprintf`. biyos writes the line nowhere else. They read only the event, so a subscriber may call them.

- `size` is the size of `buffer` in bytes. When it is at least 1, the call writes at most `size - 1` bytes of the line, then a NUL. A buffer that is too small receives the start of the line, cut at a byte, which may fall inside a UTF-8 character. A `size` of 1 receives the NUL alone.
- `*needed` receives the length of the whole line in bytes, without the NUL, whether it fits or not. The line fits when `*needed` is less than `size`, so a buffer of `*needed + 1` bytes holds it.
- When `size` is 0 the call writes nothing to `buffer`, not even a NUL, and `buffer` may be null, to learn `*needed` alone.
- The line has no trailing newline. A byte below 0x20 is written as `\n` for a newline, `\t` for a tab and `\u00XX` with lowercase hex digits for any other, such as `\u000d` for a carriage return, so the line never breaks. Each call says how it writes the other bytes.
- The time of an event is the wall time since its environment was created. It is written in seconds with six decimals, cut to the microsecond, such as `0.001234` for 1234567 nanoseconds. The decimal point is always `.`, whatever the locale.
- They take no environment, so a failure keeps no message and sends no event. They return `BIYOS_INVALID_ARGUMENT` when `event` or `needed` is null, or when `buffer` is null and `size` is not 0. A failed call writes nothing.

### `biyos_format_text`

```c
biyos_status biyos_format_text(const biyos_event *event, char *buffer, size_t size,
                               size_t *needed);
```

Writes the event for people: its time, a space, the name of its level, a space, its module and a colon, then a space and its text unless the text is empty, then for each field in order a space, its name, `=` and its value. Every byte from 0x20 up is written as it is, and nothing is quoted: for a line a program reads, use `biyos_format_json`. The error event of a failed `biyos_set_int_option`, sent 41 microseconds after the environment was created, reads

```text
0.000041 error capi: invalid argument: option 'seed': -1 is outside [0, 2147483647] code=-100
```

### `biyos_format_json`

```c
biyos_status biyos_format_json(const biyos_event *event, char *buffer, size_t size,
                               size_t *needed);
```

Writes the event for tools, as one JSON object with the members `time`, `kind`, `level`, `module`, `text` and `fields`, in that order. `time` is a JSON number, written as in text. `kind` is the name of the kind: `message` for `BIYOS_EVENT_MESSAGE`, `check` for `BIYOS_EVENT_CHECK` and `phase` for `BIYOS_EVENT_PHASE`. `level` is the name of the level: `debug`, `info`, `warning` or `error`. `fields` is an object holding the event's fields in their order, each value a string. A quote and a backslash are written as `\"` and `\\`. Bytes that are not well-formed UTF-8 become U+FFFD, one for each maximal subpart as the Unicode Standard recommends, so the line is always valid JSON. Every other byte from 0x20 up is written as it is. The same error event reads

```json
{"time":0.000041,"kind":"message","level":"error","module":"capi","text":"invalid argument: option 'seed': -1 is outside [0, 2147483647]","fields":{"code":"-100"}}
```

## Model

A model holds the variables of a problem, functions of them, and constraints and objectives on those functions. Each variable has a kind, which says what values it may take, and a lower and an upper bound. The kinds come from `biyos/model.h`.

| Macro | Value | Kind | Takes |
| --- | --- | --- | --- |
| `BIYOS_VAR_CONTINUOUS` | 1 | continuous | any value between its bounds |
| `BIYOS_VAR_BINARY` | 2 | binary | 0 or 1, between its bounds |
| `BIYOS_VAR_INTEGER` | 3 | integer | any integer between its bounds |
| `BIYOS_VAR_SEMICONTINUOUS` | 4 | semi-continuous | 0, or any value between its bounds |
| `BIYOS_VAR_SEMIINTEGER` | 5 | semi-integer | 0, or any integer between its bounds |

Every kind but continuous is discrete.

A model is made on an environment and reports there. A call on the model that fails keeps its message on that environment and sends its error event there, and while the environment delivers an event the model is read-only, as the environment is (see Subscribers). The environment cannot be freed while a model made on it is not, so a model never outlives its environment.

Variables are numbered from 0 in the order they are added, and a count or an index of variables is an `int`. The calls on variables, functions, constraints and objectives take many at once: a count and arrays of that many entries, or, for the terms of functions, arrays in the compressed form described under Functions. biyos reads them during the call and does not keep them. A call that fails changes nothing.

A call that adds items takes `int *first` as its last argument. When the call succeeds and `first` is not null, it writes to `*first` the index of the first new item, which is the number of items held before the call, a count of 0 included. The new items follow it in order.

A bound may be `INFINITY` or `-INFINITY` of `<math.h>`, and no finite bound is taken as infinite, however large. A lower bound of minus infinity or an upper bound of infinity leaves that side unbounded. A lower bound of infinity, an upper bound of minus infinity and NaN are refused, and so is a bound outside the widest bounds of the variable's kind: 0 and 1 for a binary, minus infinity and infinity for any other kind. Any other pair is kept as given, a lower bound above the upper one included.

### `biyos_model_create`

```c
biyos_status biyos_model_create(biyos_env *env, biyos_model **model);
```

Creates a model with no variables on `env` and writes it to `*model`. On failure it sets `*model` to null.

### `biyos_model_copy`

```c
biyos_status biyos_model_copy(biyos_model *model, biyos_env *env, biyos_model **copy);
```

Creates a copy of `model` on `env` and writes it to `*copy`. On failure it sets `*copy` to null. `env` may be the environment of `model` or another one. The copy holds the same variables, functions, constraints and objectives as `model`, under the same indices, and a change to either leaves the other as it was. It is a model made on `env`: it reports there, and `env` cannot be freed while it lives. Either of `model` and the copy may be freed first. The call reads `model` and changes `env`, so no other thread may use either environment while it runs. It may run while the environment of `model` delivers an event, and returns `BIYOS_INVALID_ARGUMENT` while `env` does. A failure keeps its message on the environment of `model`, as for every call on a model.

### `biyos_model_free`

```c
biyos_status biyos_model_free(biyos_model *model);
```

Frees the model. A null `model` gives `BIYOS_OK`. While the environment of `model` delivers an event it returns `BIYOS_INVALID_ARGUMENT` and frees nothing.

### `biyos_add_variables`

```c
biyos_status biyos_add_variables(biyos_model *model, int count, const int *kinds,
                                 const double *lower, const double *upper, int *first);
```

Adds `count` variables after the last one. Entry `j` of each array gives variable `n + j`, where `n` is the number of variables before the call: its kind, a `BIYOS_VAR_*` value, and its lower and upper bounds. A null `kinds` makes every new variable continuous, and a null `lower` or `upper` gives each the widest such bound of its kind. `*first` receives `n`, and a null `first` is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, a kind is unknown or a bound is refused, and `BIYOS_NO_MEMORY` when the model would hold more than 2147483647 variables.

### `biyos_set_variable_bounds`

```c
biyos_status biyos_set_variable_bounds(biyos_model *model, int count, const int *indices,
                                       const double *lower, const double *upper);
```

Sets the bounds of variable `indices[j]` to `lower[j]` and `upper[j]`, for each `j` below `count`. A null `lower` or `upper` leaves those bounds as they are, and `indices` may be null when `count` is 0. A variable may appear more than once: its entries apply in order, so the last one wins, and each must be accepted. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, an index is not that of a variable or a bound is refused.

### `biyos_set_variable_kinds`

```c
biyos_status biyos_set_variable_kinds(biyos_model *model, int count, const int *indices,
                                      const int *kinds);
```

Sets the kind of variable `indices[j]` to `kinds[j]`, for each `j` below `count`, and keeps its bounds. A null `kinds` changes nothing, and `indices` may be null when `count` is 0. A variable may appear more than once, as in `biyos_set_variable_bounds`. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, an index is not that of a variable, a kind is unknown or the bounds of a variable are refused for its new kind, as when it becomes binary with a bound outside [0, 1]: set such bounds first.

### `biyos_variable_count`

```c
biyos_status biyos_variable_count(biyos_model *model, int *count);
```

Writes the number of variables of the model to `*count`.

### `biyos_get_variables`

```c
biyos_status biyos_get_variables(biyos_model *model, int first, int count, int *kinds,
                                 double *lower, double *upper);
```

Writes to entry `j` of each array, for each `j` below `count`, the kind and the lower and upper bounds of variable `first + j`. A null array is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `first` or `count` is negative or `first + count` exceeds the number of variables.

### Functions

A function of the variables `x` is `f(x) = c + Σ a_j x_j + Σ q_ij x_i x_j`: a constant `c`, linear terms and quadratic terms. Each linear term is a variable `j`, given by its index, and its coefficient `a_j`. Each quadratic term is a pair of variables `i` and `j` and its coefficient `q_ij`, taken as written, with no factor ½: `x_0²` is the pair (0, 0) with the coefficient 1, and `x_0 x_1` the pair (0, 1) with the coefficient 1. Constraints and objectives refer to functions by index, so one function may serve several of them. Functions are numbered from 0 in the order they are added, separately from variables, and a count or an index of functions is an `int`.

A function's constant must be finite. Its linear terms may come in any order and are kept sorted by variable. Each must name a variable of the model, appear once in the function and have a finite coefficient, neither NaN nor infinite. A term whose coefficient is 0 is kept.

A function is added with no quadratic terms, and `biyos_add_quadratic_terms` adds them to functions the model holds. A pair may be given in either order and is kept as `(i, j)` with `i <= j`, a function's pairs sorted by `i`, then by `j`. Each must name variables of the model, appear once for its function in a call and have a finite coefficient. A pair the function already holds adds its coefficient to the one held, which must stay finite, so every call adds `q_ij x_i x_j` to its functions. A term whose coefficient is 0, given or summed, is kept.

The terms of many functions travel in compressed form, as three arrays. Function `j` of them has the terms from `starts[j]` to `starts[j + 1] - 1` of `variables` and `values`, which hold a variable and its coefficient each. So `starts` holds one more offset than there are functions: the first is 0, none is below the one before it, and the last is the number of terms. An offset or a number of terms is an `int64_t` of `<stdint.h>`, which `biyos/biyos.h` includes. Quadratic terms travel in the same form, with `firsts`, `seconds` and `values` holding the two variables of each pair and its coefficient.

### `biyos_add_functions`

```c
biyos_status biyos_add_functions(biyos_model *model, int count, const double *constants,
                                 const int64_t *starts, const int *variables,
                                 const double *values, int *first);
```

Adds `count` functions after the last one. Entry `j` of `constants` gives the constant of function `n + j`, where `n` is the number of functions before the call, and `starts`, `variables` and `values` give the linear terms of the functions in compressed form, `starts` holding `count + 1` offsets. A null `constants` gives every new function the constant 0. A null `starts` adds functions without terms. `variables` and `values` are not read when there are no terms, and may then be null. `*first` receives `n`, and a null `first` is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, `starts` does not begin at 0 or decreases, a constant is not finite, a variable is not one of the model or appears twice in a function, or a coefficient is not finite, and `BIYOS_NO_MEMORY` when the model would hold more than 2147483647 functions or more terms than any allocation can hold.

### `biyos_function_count`

```c
biyos_status biyos_function_count(biyos_model *model, int *count);
```

Writes the number of functions of the model to `*count`.

### `biyos_get_functions`

```c
biyos_status biyos_get_functions(biyos_model *model, int first, int count, double *constants,
                                 int64_t *starts, int64_t size, int64_t *needed, int *variables,
                                 double *values);
```

Writes the constants of the `count` functions from `first`, and their linear terms in compressed form, each function's sorted by variable, into arrays the caller gives, in the manner of the formatting calls.

- `constants` receives `count` constants, entry `j` that of function `first + j`.
- `starts` receives `count + 1` offsets from 0, whatever `size` is, so that `starts[j]` is the number of terms of the `j` functions before function `first + j`.
- `size` is the number of terms `variables` and `values` hold. The call writes at most `size` terms to each: the first `size` terms of the functions, in order, when the functions have more.
- `*needed` receives the number of terms of the functions, whether they fit or not. They fit when `*needed` is at most `size`, and function `first + j` is whole in `variables` and `values` when `starts[j + 1]` is at most `size`.
- A null `constants`, `starts`, `variables` or `values` is skipped, so a `size` of 0 with null arrays learns `*needed` alone.

Returns `BIYOS_INVALID_ARGUMENT` when `first`, `count` or `size` is negative, `first + count` exceeds the number of functions, or `needed` is null.

### `biyos_add_quadratic_terms`

```c
biyos_status biyos_add_quadratic_terms(biyos_model *model, int count, const int *functions,
                                       const int64_t *starts, const int *firsts,
                                       const int *seconds, const double *values);
```

Adds quadratic terms to `count` functions of the model. Function `functions[j]` gains the terms from `starts[j]` to `starts[j + 1] - 1` of `firsts`, `seconds` and `values`, `starts` holding `count + 1` offsets: term `k` adds `values[k]` times the product of variables `firsts[k]` and `seconds[k]`. A function appears at most once in `functions`, which has no default and may be null only when `count` is 0. A null `starts` adds no terms. `firsts`, `seconds` and `values` are not read when there are no terms, and may then be null. A call that adds new pairs moves the terms and offsets of every function from its lowest one on, so the terms of many functions are best added in one call, or those of a function right after it is added. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, a function is not one of the model or is listed twice, `starts` does not begin at 0 or decreases, a variable is not one of the model, a pair appears twice for a function in the call, or a coefficient, or its sum with the one held, is not finite, and `BIYOS_NO_MEMORY` when the model would hold more quadratic terms than any allocation can hold.

### `biyos_get_quadratic_terms`

```c
biyos_status biyos_get_quadratic_terms(biyos_model *model, int first, int count, int64_t *starts,
                                       int64_t size, int64_t *needed, int *firsts, int *seconds,
                                       double *values);
```

Writes the quadratic terms of the `count` functions from `first` in compressed form, each function's sorted by pair with `firsts[k] <= seconds[k]`, as `biyos_get_functions` writes linear terms. `starts` receives `count + 1` offsets from 0, whatever `size` is. `size` is the number of terms `firsts`, `seconds` and `values` hold, and the call writes at most `size` terms to each. `*needed` receives the number of quadratic terms of the functions, whether they fit or not. A null `starts`, `firsts`, `seconds` or `values` is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `first`, `count` or `size` is negative, `first + count` exceeds the number of functions, or `needed` is null.

### Constraints

A constraint `lower <= f(x) <= upper` bounds a function `f` of the model. It holds the index of the function and a lower and an upper bound, and several constraints may share a function. Constraints are numbered from 0 in the order they are added, separately from variables and functions, and a count or an index of constraints is an `int`. A constraint's bounds follow the rule of a continuous variable's: `INFINITY` or `-INFINITY` leaves that side unbounded, a lower bound of infinity, an upper bound of minus infinity and NaN are refused, and any other pair is kept as given, a lower bound above the upper one included. A constraint whose bounds are equal is an equality, and one whose bounds are both infinite is free.

### `biyos_add_constraints`

```c
biyos_status biyos_add_constraints(biyos_model *model, int count, const int *functions,
                                   const double *lower, const double *upper, int *first);
```

Adds `count` constraints after the last one. Entry `j` of each array gives constraint `m + j`, where `m` is the number of constraints before the call: the index of its function and its lower and upper bounds. `functions` has no default, and may be null only when `count` is 0. A null `lower` or `upper` leaves that side of every new constraint unbounded, so with both null the constraints are free. `*first` receives `m`, and a null `first` is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, a function is not one of the model or a bound is refused, and `BIYOS_NO_MEMORY` when the model would hold more than 2147483647 constraints.

### `biyos_set_constraint_bounds`

```c
biyos_status biyos_set_constraint_bounds(biyos_model *model, int count, const int *indices,
                                         const double *lower, const double *upper);
```

Sets the bounds of constraint `indices[j]` to `lower[j]` and `upper[j]`, for each `j` below `count`, and keeps its function. A null `lower` or `upper` leaves those bounds as they are, and `indices` may be null when `count` is 0. A constraint may appear more than once, as in `biyos_set_variable_bounds`. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, an index is not that of a constraint or a bound is refused.

### `biyos_constraint_count`

```c
biyos_status biyos_constraint_count(biyos_model *model, int *count);
```

Writes the number of constraints of the model to `*count`.

### `biyos_get_constraints`

```c
biyos_status biyos_get_constraints(biyos_model *model, int first, int count, int *functions,
                                   double *lower, double *upper);
```

Writes to entry `j` of each array, for each `j` below `count`, the function and the lower and upper bounds of constraint `first + j`. A null array is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `first` or `count` is negative or `first + count` exceeds the number of constraints.

### Objectives

An objective is a function of the model to minimize or maximize, with a priority, a weight and an absolute and a relative tolerance. Objectives refer to functions by index, so several may share a function, and an objective may use the function of a constraint. Objectives are numbered from 0 in the order they are added, separately from variables, functions and constraints, and a count or an index of objectives is an `int`. The senses come from `biyos/model.h`.

| Macro | Value | Sense |
| --- | --- | --- |
| `BIYOS_MINIMIZE` | 1 | the function is minimized |
| `BIYOS_MAXIMIZE` | 2 | the function is maximized |

The objectives are optimized in stages, one for each priority they hold, the lowest priority first. A stage minimizes the weighted sum `Σ w_i s_i f_i(x)` of the objectives of its priority, where `w_i` is the weight of objective `i`, `s_i` is 1 when it minimizes and -1 when it maximizes, and `f_i` is its function. Each later stage keeps every objective of an earlier stage within its tolerances of the optimum found for it, the absolute tolerance in the units of its function and the relative one as a fraction of the size of that optimum. So objectives of distinct priorities are optimized in sequence, and objectives of one priority are blended. A priority is any `int`. A weight is finite and positive, so the sense alone gives the direction. A tolerance is 0 or more, and `INFINITY` sets no limit. No call optimizes the objectives yet, since biyos solves nothing.

### `biyos_add_objectives`

```c
biyos_status biyos_add_objectives(biyos_model *model, int count, const int *functions,
                                  const int *senses, const int *priorities, const double *weights,
                                  const double *absolute, const double *relative, int *first);
```

Adds `count` objectives after the last one. Entry `j` of each array gives objective `p + j`, where `p` is the number of objectives before the call: the index of its function, its sense, a `BIYOS_MINIMIZE` or `BIYOS_MAXIMIZE` value, its priority, its weight and its absolute and relative tolerances. `functions` has no default, and may be null only when `count` is 0. A null `senses` makes every new objective minimize, a null `priorities` gives objective `p + j` the priority `p + j`, a null `weights` gives each the weight 1, and a null `absolute` or `relative` gives each that tolerance 0. So by default the objectives are optimized one after another in the order they are added, and objectives given one priority are blended. `*first` receives `p`, and a null `first` is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, a function is not one of the model, a sense is unknown, a weight is not finite and positive or a tolerance is NaN or negative, and `BIYOS_NO_MEMORY` when the model would hold more than 2147483647 objectives.

### `biyos_set_objectives`

```c
biyos_status biyos_set_objectives(biyos_model *model, int count, const int *indices,
                                  const int *senses, const int *priorities, const double *weights,
                                  const double *absolute, const double *relative);
```

Sets the sense, the priority, the weight and the absolute and relative tolerances of objective `indices[j]` to `senses[j]`, `priorities[j]`, `weights[j]`, `absolute[j]` and `relative[j]`, for each `j` below `count`, and keeps its function. A null `senses`, `priorities`, `weights`, `absolute` or `relative` leaves those values as they are, and `indices` may be null when `count` is 0. An objective may appear more than once, as in `biyos_set_variable_bounds`. Returns `BIYOS_INVALID_ARGUMENT` when `count` is negative, an index is not that of an objective, a sense is unknown, a weight is not finite and positive or a tolerance is NaN or negative.

### `biyos_objective_count`

```c
biyos_status biyos_objective_count(biyos_model *model, int *count);
```

Writes the number of objectives of the model to `*count`.

### `biyos_get_objectives`

```c
biyos_status biyos_get_objectives(biyos_model *model, int first, int count, int *functions,
                                  int *senses, int *priorities, double *weights, double *absolute,
                                  double *relative);
```

Writes to entry `j` of each array, for each `j` below `count`, the function, the sense, the priority, the weight and the absolute and relative tolerances of objective `first + j`. A null array is skipped. Returns `BIYOS_INVALID_ARGUMENT` when `first` or `count` is negative or `first + count` exceeds the number of objectives.

## Expressions

`biyos/expr.h` numbers the operators of expressions, which form the nonlinear parts of functions. An expression is a node of the model's graph, an operator applied to its children, and a node that several parents share is stored once. No call takes the operators yet.

| Macro | Value | Node | Children |
| --- | --- | --- | --- |
| `BIYOS_OP_CONSTANT` | 1 | a constant `c` | none |
| `BIYOS_OP_VARIABLE` | 2 | a variable `x_j` | none |
| `BIYOS_OP_SUM` | 3 | `c + a_1 e_1 + ... + a_n e_n`, with a constant `c` and a coefficient `a_i` for each child `e_i` | one or more |
| `BIYOS_OP_PRODUCT` | 4 | `c e_1 ... e_n`, with a coefficient `c` | one or more |
| `BIYOS_OP_NEGATION` | 5 | `-e` | one |
| `BIYOS_OP_DIVISION` | 6 | `e_1 / e_2` | two |
| `BIYOS_OP_CONSTANT_POWER` | 7 | `e^c`, with a constant exponent `c` | one |
| `BIYOS_OP_POWER` | 8 | `e_1^e_2` | two |
| `BIYOS_OP_EXP` | 9 | `exp(e)` | one |
| `BIYOS_OP_LOG` | 10 | `log(e)`, the natural logarithm | one |
| `BIYOS_OP_SQRT` | 11 | `sqrt(e)` | one |
| `BIYOS_OP_SIN` | 12 | `sin(e)` | one |
| `BIYOS_OP_COS` | 13 | `cos(e)` | one |
| `BIYOS_OP_TAN` | 14 | `tan(e)` | one |
| `BIYOS_OP_ABS` | 15 | `abs(e)`, the absolute value | one |
| `BIYOS_OP_MAX` | 16 | `max(e_1, ..., e_n)` | one or more |
| `BIYOS_OP_MIN` | 17 | `min(e_1, ..., e_n)` | one or more |
| `BIYOS_OP_ASIN` | 18 | `asin(e)` | one |
| `BIYOS_OP_ACOS` | 19 | `acos(e)` | one |
| `BIYOS_OP_ATAN` | 20 | `atan(e)` | one |
| `BIYOS_OP_SINH` | 21 | `sinh(e)` | one |
| `BIYOS_OP_COSH` | 22 | `cosh(e)` | one |
| `BIYOS_OP_TANH` | 23 | `tanh(e)` | one |
| `BIYOS_OP_ASINH` | 24 | `asinh(e)` | one |
| `BIYOS_OP_ACOSH` | 25 | `acosh(e)` | one |
| `BIYOS_OP_ATANH` | 26 | `atanh(e)` | one |
| `BIYOS_OP_ENTROPY` | 27 | `e log(e)` | one |
| `BIYOS_OP_SIGN_POWER` | 28 | `sign(e) abs(e)^c`, with a constant exponent `c > 0` | one |
| `BIYOS_OP_ERF` | 29 | `erf(e)`, the error function | one |

Every constant, coefficient and exponent of an expression is finite.

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
