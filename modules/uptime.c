#include <kernel/sched.h>
#include <kernel/printf.h>

void main(void) {
    int hours = 0, minutes = 0, seconds = 0;
    for (;;) {
        if (seconds >= 60) {
            seconds = 0;
            minutes++;
        }
        if (minutes >= 60) {
            minutes = 0;
            hours++;
        }

        printf("\rUptime: %dh, %dmin, %ds", hours, minutes, seconds++);
        sched_sleep(1000000);
    }
}