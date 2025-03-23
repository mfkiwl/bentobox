#pragma once
#include <stdatomic.h>

void acquire(atomic_flag *lock);
void release(atomic_flag *lock);