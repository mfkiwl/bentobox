#include <kernel/sched.h>
#include <kernel/pci.h>
#include <kernel/vfs.h>
#include <kernel/elf64.h>
#include <kernel/printf.h>
#include <kernel/version.h>

#include <kernel/ksym.h>

extern void generic_load_modules(void);
extern void debugcon_entry(void);

void generic_startup(void) {
    vfs_install();
	pci_scan();
    generic_load_modules();
	sched_install();
    sched_new_task(debugcon_entry, "bentobox debug shell", -1);
}

void generic_main(void) {
    printf("\nWelcome to \033[96mbentobox\033[0m!\n%s %d.%d-%s %s %s %s\n\n",
        __kernel_name, __kernel_version_major,__kernel_version_minor,
        __kernel_commit_hash, __kernel_build_date, __kernel_build_time, __kernel_arch);

	sched_start_all_cores();
}