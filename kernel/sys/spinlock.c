#include <stdatomic.h>

extern void generic_pause();

void acquire(atomic_flag *lock) {
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        generic_pause();
    }
}

void release(atomic_flag *lock) {
    atomic_flag_clear_explicit(lock, memory_order_release);
}