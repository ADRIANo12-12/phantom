# Linux Kernel / Phantom — Workqueue & OSD Notes

## `struct work_struct`

A `struct work_struct` represents one unit of deferred work handled by the kernel workqueue system.

```c
#include <linux/workqueue.h>

static struct work_struct work;
```

An object and a pointer are different:

```c
struct work_struct work;    /* actual object */
struct work_struct *pwork;  /* pointer to an object */
```

For an object, its address is `&work`. `&pwork` would be a pointer to the pointer and is not what `schedule_work()` expects.

## Callback

A work item needs a callback. The callback receives the work item pointer:

```c
static void osd_work(struct work_struct *work)
{
    /* deferred work */
}
```

## Initialization

Connect the work item to its callback with `INIT_WORK`:

```c
INIT_WORK(&work, osd_work);
```

Conceptually:

```text
work_struct -> osd_work()
```

## Scheduling

Queue the work on the kernel's default workqueue:

```c
schedule_work(&work);
```

Flow:

```text
schedule_work()
      -> workqueue
      -> kernel worker
      -> osd_work()
```

The same work item can be scheduled again after its previous execution has completed. It must not be treated as a queue of multiple independent jobs.

## Cleanup

When the lifetime of the work item is ending, synchronize with it:

```c
cancel_work_sync(&work);
```

This is especially important for unloadable modules so the callback cannot outlive the code/data it uses.

## `__init`

`__init` marks initialization code in the Linux kernel. It is commonly used for functions that only need to run during initialization.

```c
static int __init psysinfo_init(void)
{
    /* initialization */
    return 0;
}
```

An initcall such as `subsys_initcall(psysinfo_init)` schedules that function for the corresponding kernel initialization stage.

## `__sched`

`__sched` is an annotation used by Linux kernel code for functions associated with the scheduler. It is not the same thing as `schedule()` or `schedule_work()`.

```text
__sched         -> annotation
schedule()      -> scheduler operation
schedule_work() -> queue deferred work
```

## Kernel version

```c
#include <linux/utsname.h>

init_uts_ns.name.release
```

`init_uts_ns.name.release` provides the kernel release string.

Other UTS information is available through `init_uts_ns.name` as well.

## Kernel time

```c
#include <linux/timekeeping.h>

struct timespec64 ts;
ktime_get_real_ts64(&ts);
```

`ts.tv_sec` is seconds since the Unix epoch; it is **not** directly an `HH:MM:SS` clock value.

For a human-readable clock, convert the timestamp into calendar/time fields and then use the hour/minute/second fields.

## Boolean state

Kernel code can use `bool` with `true` and `false`:

```c
bool kern_env_is_set_up;
kern_env_is_set_up = true;
```

Remember the difference between assignment and comparison:

```text
=   -> assign a value
==  -> compare values
```

An `if` statement is executable code and therefore belongs inside a function; it cannot simply appear at file scope.

## OSD work design

For a Phantom OSD information system, a useful structure is:

```text
initialization
    -> initialize work_struct
    -> schedule work
    -> worker callback
        -> collect/update OSD information
        -> kernel version
        -> time
        -> Phantom state
```

Keep the actual `work_struct` as part of a longer-lived object when practical, rather than using an uninitialized pointer.

## Common C mistakes from the first OSD prototype

- `if (state = true)` assigns instead of checking.
- Calling functions such as `ktime_get_real_ts64()` at file scope is invalid; function calls belong inside functions.
- `struct work_struct *pwork;` declares only a pointer; it does not create a `work_struct` object.
- `schedule_work(&pwork)` passes a `struct work_struct **` when a `struct work_struct *` is expected.
- Declaring a callback does not execute it; it must be attached with `INIT_WORK` and queued with `schedule_work`.
- Avoid unnecessary kernel headers. Include the headers that provide the APIs/types actually used.
- The SPDX tag is written as `SPDX-License-Identifier: GPL-2.0`.
