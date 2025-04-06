#include <stdatomic.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/sched.h>
#include <kernel/mutex.h>
#include <kernel/malloc.h>
#include <kernel/spinlock.h>

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner = NULL;
    m->queue = NULL;
}

void mutex_lock(mutex_t *m) {
    acquire(&(m->lock));
    struct cpu *this = this_core();

    if (!this->current_proc || m->owner == this->current_proc) {
        release(&(m->lock));
        return;
    }
    
    while (m->locked) {
        if (!m->queue) {
            m->queue = (mutex_list_t *)kmalloc(sizeof(mutex_list_t));
            m->queue->proc = this->current_proc;
            m->queue->next = NULL;
        } else {
            mutex_list_t *i = m->queue;
            while (i->next) {
                i = i->next;
            }
            i->next = (mutex_list_t *)kmalloc(sizeof(mutex_list_t));
            i->next->proc = this->current_proc;
            i->next->next = NULL;
        }
        sched_block(MUTEX);
    }

    m->locked = 1;
    m->owner = this->current_proc;
    
    release(&(m->lock));
}

void mutex_unlock(mutex_t *m) {
    acquire(&m->lock);
    struct cpu *this = this_core();

    if (!this->current_proc || m->owner != this->current_proc) {
        release(&m->lock);
        return;
    }

    m->locked = 0;
    m->owner = NULL;

    if (m->queue != NULL) {
        mutex_list_t *first_thread = m->queue;
        m->queue = first_thread->next;
        sched_unblock(first_thread->proc); 
        kfree(first_thread);
    }

    release(&m->lock);
}