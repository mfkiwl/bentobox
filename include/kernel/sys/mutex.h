#pragma once
#include <stdatomic.h>
#include <kernel/sys/sched.h>

typedef struct mutex_list {
    struct task *proc;
    struct mutex_list *next;
    struct mutex_list *last;
} mutex_list_t;

typedef struct mutex {
    int locked;
    struct task *owner;
    struct mutex_list *queue;
    atomic_flag lock;
} mutex_t;