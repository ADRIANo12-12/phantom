// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_INSTALLER_H
#define PHANTOM_INSTALLER_H

#include <stdint.h>

int phantom_installer_run(void);

int phantom_installer_do_system_check(void);
int phantom_installer_do_network_setup(void);
int phantom_installer_do_disk_setup(void);
int phantom_installer_do_install(void);

/* Callback postępu: percent 0-100, message */
typedef void (*phantom_progress_fn)(uint32_t percent, const char *msg, void *user);

int phantom_installer_do_install_with_progress(phantom_progress_fn cb, void *user);
int phantom_installer_do_system_check_with_progress(phantom_progress_fn cb, void *user);
int phantom_installer_do_network_setup_with_progress(phantom_progress_fn cb, void *user);
int phantom_installer_do_disk_setup_with_progress(phantom_progress_fn cb, void *user);

#endif
