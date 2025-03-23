#include <stdatomic.h>
#include <kernel/heap.h>
#include <kernel/mutex.h>
#include <kernel/sched.h>
#include <kernel/spinlock.h>

mutex_t *mutex_create(void) {
    mutex_t *m = (mutex_t *)kmalloc(sizeof(mutex_t));
    m->locked = 0;
    m->owner = NULL;
    m->queue = NULL;
    return m;
}

void mutex_lock(mutex_t *m) {
    acquire(&(m->lock));
    
    if (m->owner == current_proc) {
        release(&(m->lock));
        return;
    }
    
    while (m->locked) {
        if (!m->queue) {
            m->queue = (mutex_list_t *)kmalloc(sizeof(mutex_list_t));
            m->queue->proc = current_proc;
            m->queue->next = NULL;
        } else {
            mutex_list_t *i = m->queue;
            while (i->next) {
                i = i->next;
            }
            i->next = (mutex_list_t *)kmalloc(sizeof(mutex_list_t));
            i->next->proc = current_proc;
            i->next->next = NULL;
        }
        sched_block(MUTEX);
    }

    m->locked = 1;
    m->owner = current_proc;
    
    release(&(m->lock));
}

void mutex_unlock(mutex_t *m) {
    acquire(&m->lock);

    if (m->owner != current_proc) {
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