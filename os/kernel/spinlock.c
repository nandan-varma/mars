#include "spinlock.h"
#include <stdatomic.h>

void spinlock_init(spinlock_t *lock) {
    atomic_init(&lock->locked, 0);
    lock->owner = 0;
}

void spinlock_acquire(spinlock_t *lock) {
    while (atomic_exchange(&lock->locked, 1) == 1) {
    }
    lock->owner = 1;
}

void spinlock_release(spinlock_t *lock) {
    lock->owner = 0;
    atomic_store(&lock->locked, 0);
}
