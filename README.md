# Phantom OS

<p align="center">
  <strong>A Linux-based operating system built from the kernel up.</strong><br>
  Low-level development • Custom kernel • Custom userspace • Custom boot experience
</p>

---

## Overview

**Phantom OS** is an experimental operating system project built around a heavily customized Linux kernel and a growing Phantom-specific userspace and system environment.

The project focuses on understanding and controlling the operating system stack from low-level components upward:

```text
Firmware / Bootloader
        ↓
      Kernel
        ↓
 Phantom Kernel
        ↓
     Init / PID 1
        ↓
   Userspace
        ↓
 System Services
        ↓
    Applications

Phantom is not intended to remain simply a Linux distribution with a modified theme.

The long-term goal is to build a distinct operating system environment with its own components, interfaces, system behavior and development philosophy while using Linux as the initial kernel foundation.

Project Status

Status: Active development

Phantom OS is currently in an early development stage.

The project can already:

boot a custom-built kernel
boot a custom initramfs
execute Phantom-specific kernel code
run userspace through BusyBox
display a custom Phantom boot splash
display a custom Phantom panic screen
automatically reboot after a kernel panic
run and test the system inside QEMU
build Phantom-specific kernel code directly into the kernel

The project is not production-ready.

Goals

Phantom is being developed around several major goals.

1. Low-level control

Understand and control the system as close to the hardware as practical:

CPU
memory
interrupts
scheduling
device management
drivers
filesystems
networking
system calls
boot process
2. Custom operating-system environment

Phantom should eventually have its own:

system services
userspace tools
shell
system management
installer
package management
configuration system
boot experience
error handling
3. Learn by building

Phantom is also a learning project.

Instead of simply consuming existing abstractions, the project is intended to progressively investigate how the underlying operating system works and replace or extend components where there is a practical reason to do so.

Current Architecture

The current project can be visualized as:

                    PHANTOM OS
                        │
        ┌───────────────┴───────────────┐
        │                               │
     Kernel                         Userspace
        │                               │
  ┌─────┴─────┐                    ┌────┴────┐
  │           │                    │         │
 Linux     Phantom              BusyBox    /init
 Kernel    subsystems               │
  │           │                     │
  └─────┬─────┘                     │
        │                           │
        └────────────┬──────────────┘
                     │
                Initramfs
                     │
                     ↓
                   QEMU

This architecture will evolve significantly as the project grows.

Repository Structure

The repository is organized around the major parts of the operating system.

phantom/
│
├── kernel/
│   ├── arch/
│   ├── drivers/
│   ├── fs/
│   ├── include/
│   ├── mm/
│   ├── net/
│   └── ...
│
├── phantom/
│   ├── ...
│   └── ...
│
├── rootfs/
│   └── ...
│
├── installer/
│   └── ...
│
├── iso/
│   └── ...
│
├── docs/
│   └── ...
│
└── README.md

The structure is still evolving. Some directories shown above represent planned components rather than fully implemented subsystems.

Kernel

The current Phantom kernel is based on the Linux kernel.

The kernel is built directly from source and extended with Phantom-specific components.

The kernel tree currently contains a dedicated Phantom subsystem:

phantom/
├── Kconfig
├── Makefile
├── phantomsysinfo.c
└── phantomsysinfo.h

The subsystem is integrated into the Linux Kbuild system and compiled directly into the kernel.

The current build path is:

phantomsysinfo.c
        ↓
phantomsysinfo.o
        ↓
phantom/built-in.a
        ↓
built-in.a
        ↓
vmlinux
        ↓
bzImage

This means Phantom kernel functionality is currently built into the kernel, rather than being loaded later as a module.

Phantom System Information

The first Phantom-specific kernel component is phantomsysinfo.

Its current purpose is to establish a Phantom-owned subsystem that executes during kernel initialization.

The current initialization path is conceptually:

Kernel boot
    ↓
Kbuild-built Phantom subsystem
    ↓
psysinfo_init()
    ↓
Phantom kernel message

The subsystem currently confirms its initialization through the kernel log.

Example:

Phantom information system has been setup!

This is intentionally the first step rather than the final design.

Future versions are expected to expose real system information such as:

CPU information
memory information
architecture
kernel information
hardware information
runtime system state
Initramfs

Phantom currently uses a custom initramfs during development.

The initramfs contains the initial userspace environment required to start Phantom after the kernel has booted.

The development environment currently uses:

/init
/bin/
BusyBox

The /init program is responsible for:

mounting required virtual filesystems
preparing /dev
configuring the initial environment
displaying the Phantom boot screen
starting the initial shell/userspace

The initramfs is currently generated during development and supplied to QEMU together with the kernel.

BusyBox

BusyBox is currently used as the initial userspace foundation.

This provides basic utilities such as:

sh
ls
cat
clear
mount
mkdir
dmesg
reboot
poweroff

and other core commands required during early development.

BusyBox is currently a practical bootstrap layer.

The long-term goal is to gradually introduce Phantom-specific userspace components where appropriate.

Boot Process

The current development boot flow looks approximately like this:

QEMU
  ↓
Linux kernel / Phantom kernel
  ↓
kernel initialization
  ↓
Phantom subsystem initialization
  ↓
initramfs
  ↓
/init
  ↓
Phantom boot splash
  ↓
userspace

The development environment currently uses a quiet kernel configuration to prevent unnecessary kernel output from overwhelming the Phantom interface.

Phantom Boot Splash

Phantom has a custom textual boot splash.

The purpose of the splash is to eventually hide low-level boot noise from normal users and present a clean Phantom startup experience.

The intended boot flow is:

Boot
 ↓
Phantom OS splash
 ↓
System initialization
 ↓
Userspace

The development version currently operates in the terminal because the project is being tested through QEMU -nographic.

Phantom Panic Screen

Phantom also has a custom kernel panic presentation.

Instead of exposing only the traditional Linux panic presentation, Phantom provides its own user-facing error screen.

Conceptually:

                 P H A N T O M   O S


        Oops! Your system encountered an error
        that couldn't be fixed!

        We're finding the reason about what
        started this problem.


        SYSTEM HALTED        YOU CAN SAFELY REBOOT


              Rebooting in 10 seconds...

The current design also includes a small Tux ASCII easter egg.

The panic path uses the kernel panic infrastructure rather than implementing an unrelated userspace crash mechanism.

Automatic reboot is currently configured during development.

QEMU

QEMU is the primary development and testing environment.

Phantom is intentionally tested inside a virtual machine before being considered for real hardware.

The normal development workflow uses:

-nographic

This keeps the system entirely inside the terminal and makes kernel development easier to control.

Example:

qemu-system-x86_64 \
    -kernel kernel/arch/x86/boot/bzImage \
    -initrd phantom-rootfs.cpio.gz \
    -append "console=ttyS0" \
    -nographic

The exact command may change as the project develops.

Development Environment

The current development environment is:

Windows
   ↓
WSL
   ↓
Linux development environment
   ↓
Phantom source tree
   ↓
Kernel build
   ↓
QEMU

The real hardware is intentionally separated from early kernel experiments.

The goal is:

Experiment
    ↓
Build
    ↓
QEMU test
    ↓
Fix
    ↓
QEMU test again
    ↓
Only then consider real hardware
Build

The kernel can currently be built with:

cd kernel
make -j6

The resulting x86 kernel image is:

kernel/arch/x86/boot/bzImage

The project currently uses six parallel build jobs during development.

Git Branches

Phantom uses two primary branches.

master
   ↓
active development

main
   ↓
stable / release-ready version
master

master is the active development branch.

This is where:

new kernel code is developed
experiments are performed
features are added
architecture changes happen
QEMU testing is performed

The branch may temporarily contain incomplete or experimental functionality.

main

main represents the stable Phantom version.

Changes should reach main only after they have been developed and tested on master.

The intended workflow is:

master
   ↓
development
   ↓
testing
   ↓
stabilization
   ↓
main
   ↓
release
Development Philosophy

Phantom follows several rules.

Build something real

A Phantom component should eventually have a real purpose.

The project intentionally avoids creating code whose only purpose is to print:

Hello Phantom

and remain otherwise unused.

The current phantomsysinfo subsystem is the foundation for a future system-information interface.

Understand before replacing

Linux already solves many difficult systems problems.

When Phantom needs functionality that already exists in Linux, the preferred approach is:

Understand existing implementation
        ↓
Understand kernel APIs
        ↓
Build Phantom-specific abstraction where useful
        ↓
Replace or extend components when justified

The goal is learning and control, not unnecessary rewrites.

Develop incrementally

Phantom should remain bootable throughout development whenever possible.

The preferred cycle is:

small change
   ↓
compile
   ↓
boot in QEMU
   ↓
test
   ↓
commit

instead of making hundreds of changes before testing.

Development Workflow

Typical development:

cd ~/phantom
git switch master

Work on the system.

Build:

cd kernel
make -j6

Test:

QEMU → -nographic

Then return to the project root:

cd ~/phantom

Review:

git status

Commit:

git add .
git commit -m "phantom: describe change"

Push:

git push origin master

When a version is considered stable:

master
   ↓
merge
   ↓
main
Roadmap
Kernel
 Build custom kernel
 Boot custom kernel
 Integrate Phantom-specific kernel directory
 First Phantom kernel subsystem
 Kernel initialization hook
 Custom kernel message
 Custom panic screen
 Automatic panic reboot
 Phantom system information API
 CPU information
 Memory information
 Hardware information
 Phantom logging subsystem
 Phantom system management interfaces
 Custom device management
 Further kernel subsystem development
Userspace
 Custom initramfs
 BusyBox
 /init
 Basic utilities
 Phantom shell
 Phantom system utilities
 Service management
 Package management
 User management
 System configuration
Boot
 Kernel boot
 Custom boot splash
 Quiet boot development mode
 Custom panic screen
 GRUB integration
 Bootloader configuration
 Hardware boot testing
 Installer boot environment
Installer

Planned installer:

PHANTOM OS INSTALLER

Welcome to Phantom OS

[ Install Phantom OS ]
[ Boot existing system ]
[ Advanced options ]

        ↓

Disk selection
        ↓
Partitioning
        ↓
User configuration
        ↓
System installation
        ↓
Bootloader installation
        ↓
First boot

The installer will eventually become a dedicated Phantom component.

Storage

Planned:

 Filesystem strategy
 System layout
 Configuration storage
 Package storage
 Recovery environment
Networking

Planned:

 Network initialization
 Network configuration
 Userspace network tools
 Network service management
Documentation

The repository is intended to grow a dedicated documentation tree:

docs/
├── architecture/
├── kernel/
├── userspace/
├── boot/
├── installer/
├── development/
└── troubleshooting/

The README is the project overview.

Detailed implementation documentation should eventually live in docs/ rather than turning the README into an enormous wall of text. GitHub supports relative links between these files, making the README a useful navigation hub.

Contributing

Phantom is currently primarily developed as an individual project.

As the project matures, contribution guidelines will be added.

Potential contribution areas include:

kernel development
C
Rust
userspace development
shell tooling
drivers
filesystems
networking
documentation
testing
installer development
Project Principles

Phantom aims to follow these principles:

Low-level first.
Understand the system.
Keep experiments reproducible.
Test before touching real hardware.
Prefer useful components over decorative code.
Document important architectural decisions.
Keep stable releases separate from development.
Current Development Target

The immediate development direction is to turn the initial Phantom kernel subsystem into a real system-information component.

Current:

phantomsysinfo
      ↓
kernel initialization
      ↓
confirmation through dmesg

Target:

phantomsysinfo
      ↓
system information
      ↓
kernel API
      ↓
userspace interface
      ↓
Phantom system utilities

This will become one of the first genuinely useful Phantom-specific pieces of the operating system.

License

Phantom currently contains and builds upon Linux kernel code.

Linux components are licensed according to the licenses included in the source tree.

See:

COPYING
LICENSES/

for the applicable licenses.

Phantom-specific source files retain their individual license headers.

Author

Adrian Sikora

Project Lead • Maintainer • Founder

Phantom OS

2026
