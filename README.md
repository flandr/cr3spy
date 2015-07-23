# Kernel module for probing cr3

Extracted from old student code for [Virtual Machine-Provided Context
Sensitive Page
Mappings](ftp://ftp.cs.wisc.edu/paradyn/papers/Rosenblum08cspm.pdf), from _way
back_. This project simply defines a kernel module that allows probing the
process's `cr3` register (i.e., the _page table base register_) via character
device.

## Usage

Requires, of course, Linux kernel headers for LKM development. Building &
installing the module are straightforward:

    make
    insmod ./cr3spy_mod.ko
    mknod /dev/cr3spy c 200 0

The probe subdirectory reads the `cr3` register value via the character device
and writes it to stdout.

## License

MIT? Sure, MIT. I guess also Copyright © 2007-2015 Nathan Rosenblum.
