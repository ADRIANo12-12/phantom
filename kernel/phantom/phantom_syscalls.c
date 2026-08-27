// SPDX-License-Identifier: GPL-2.0
//
// Phantom userspace syscall interface
//

#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(phantom_panic)
{
    panic("Phantom: userspace requested kernel panic");
}
