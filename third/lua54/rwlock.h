#ifndef _RWLOCK_H___
#define _RWLOCK_H___

/* read write lock */

#ifdef _MSC_VER
#include <Windows.h>

struct rwlock {
    SRWLOCK srw;
};

static inline void
rwlock_init(struct rwlock *lock) {
    InitializeSRWLock(&lock->srw);
}

static inline void
rwlock_rlock(struct rwlock *lock) {
    AcquireSRWLockShared(&lock->srw);
}

static inline void
rwlock_runlock(struct rwlock *lock) {
    ReleaseSRWLockShared(&lock->srw);
}

static inline void
rwlock_wlock(struct rwlock *lock) {
    AcquireSRWLockExclusive(&lock->srw);
}

static inline void
rwlock_wunlock(struct rwlock *lock) {
    ReleaseSRWLockExclusive(&lock->srw);
}

#else
struct rwlock {
    int write;
    int read;
};

static inline void
rwlock_init(struct rwlock *lock) {
    lock->write = 0;
    lock->read = 0;
}

static inline void
rwlock_rlock(struct rwlock *lock) {
    for (;;) {
        while (lock->write) {
            __sync_synchronize();
        }
        __sync_add_and_fetch(&lock->read, 1);
        if (lock->write) {
            __sync_sub_and_fetch(&lock->read, 1);
        }
        else {
            break;
        }
    }
}

static inline void
rwlock_wlock(struct rwlock *lock) {
    while (__sync_lock_test_and_set(&lock->write, 1)) {}
    while (lock->read) {
        __sync_synchronize();
    }
}

static inline void
rwlock_wunlock(struct rwlock *lock) {
    __sync_lock_release(&lock->write);
}

static inline void
rwlock_runlock(struct rwlock *lock) {
    __sync_sub_and_fetch(&lock->read, 1);
}
#endif
#endif
