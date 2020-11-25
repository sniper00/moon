#ifndef  _SPINLOCK_H__
#define _SPINLOCK_H__

#ifdef _MSC_VER
#include <Windows.h>
struct spinlock {
    LONG lock;
};

static inline void
spinlock_init(struct spinlock *lock) {
    lock->lock = 0;
}

static inline void
spinlock_lock(struct spinlock *lock) {
    while (InterlockedCompareExchange((LONG volatile *)&lock->lock, 1, 0)) {};
}

static inline int
spinlock_trylock(struct spinlock *lock) {
    return (InterlockedCompareExchange((LONG volatile *)&lock->lock, 1, 0) == 0);
}

static inline void
spinlock_unlock(struct spinlock *lock) {
    InterlockedExchange((LONG volatile *)&lock->lock, 0);
}

static inline void
spinlock_destroy(struct spinlock *lock) {
    (void)lock;
}

#else
struct spinlock {
    int lock;
};

static inline void
spinlock_init(struct spinlock *lock) {
    lock->lock = 0;
}

static inline void
spinlock_lock(struct spinlock *lock) {
    while (__sync_lock_test_and_set(&lock->lock, 1)) {}
}

static inline int
spinlock_trylock(struct spinlock *lock) {
    return __sync_lock_test_and_set(&lock->lock, 1) == 0;
}

static inline void
spinlock_unlock(struct spinlock *lock) {
    __sync_lock_release(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock *lock) {
    (void)lock;
}

#endif

#define SPIN_INIT(Q) spinlock_init(&(Q)->lock);
#define SPIN_LOCK(Q) spinlock_lock(&(Q)->lock);
#define SPIN_UNLOCK(Q) spinlock_unlock(&(Q)->lock);
#define SPIN_TRYLOCK(Q) spinlock_trylock(&(Q)->lock);

#endif