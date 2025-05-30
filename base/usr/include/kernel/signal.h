#pragma once
#include <kernel/sched.h>

#define SIGCHLD 1
#define SIGINT  2

void send_signal(struct task *proc, int signal, int extra);