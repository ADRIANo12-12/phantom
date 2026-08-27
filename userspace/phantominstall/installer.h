// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_INSTALLER_H
#define PHANTOM_INSTALLER_H

int phantom_installer_run(void);

int phantom_installer_do_system_check(void);
int phantom_installer_do_network_setup(void);
int phantom_installer_do_disk_setup(void);
int phantom_installer_do_install(void);

#endif
