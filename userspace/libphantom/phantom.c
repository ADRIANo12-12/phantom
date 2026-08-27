// SPDX-License-Identifier: GPL-2.0

#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_phantom_panic
#define __NR_phantom_panic 473
#endif

void phantom_panic(void)
{
	(void)syscall(__NR_phantom_panic);
}
