#ifndef  _SPINLOCK_H__
#define _SPINLOCK_H__

#include "atomic.h"

static inline void
spinlock_init(struct spinlock *lock) {
	lock->lock = 0;
}

static inline void
spinlock_lock(struct spinlock *lock) {
    atom_spinlock(&lock->lock);
}

static inline int
spinlock_trylock(struct spinlock *lock) {
    return atom_spintrylock(&lock->lock);
}

static inline void
spinlock_unlock(struct spinlock *lock) {
    atom_spinunlock(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock *lock) {
    (void)lock;
}

#define SPIN_INIT(Q) spinlock_init(&(Q)->lock);
#define SPIN_LOCK(Q) spinlock_lock(&(Q)->lock);
#define SPIN_UNLOCK(Q) spinlock_unlock(&(Q)->lock);
#define SPIN_TRYLOCK(Q) spinlock_trylock(&(Q)->lock);

#endif