#include <kernel/sys/sched.h>
#include <kernel/pci.h>
#include <kernel/printf.h>
#include <kernel/version.h>

void generic_startup(void) {
	sched_install();
	pci_scan();
}

void generic_main(void) {
    printf("\nWelcome to \033[96mbentobox\033[0m!\n%s %d.%d %s %s %s\n\n",
        __kernel_name, __kernel_version_major,__kernel_version_minor,
        __kernel_build_date, __kernel_build_time, __kernel_arch);

	sched_yield();
}