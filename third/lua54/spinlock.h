#ifndef  _SPINLOCK_H__
#define _SPINLOCK_H__

#if defined(_MSC_VER) && !defined(__cplusplus)
#include <stdbool.h>
#include <intrin.h>

#define ATOMIC_FLAG_INIT_ { 0 }

typedef struct atomic_flag { bool _Value; } atomic_flag_;

inline bool atomic_flag_test_and_set_(volatile atomic_flag_* flag)
{
    return _InterlockedExchange8((volatile char*)flag, 1) == 1;
}

inline void atomic_flag_clear_(volatile atomic_flag_* flag)
{
    _InterlockedExchange8((volatile char*)flag, 0);
}

#else
#include <stdatomic.h>
#define atomic_flag_ atomic_flag
#define ATOMIC_FLAG_INIT_ ATOMIC_FLAG_INIT
#define atomic_flag_test_and_set_ atomic_flag_test_and_set
#define atomic_flag_clear_ atomic_flag_clear
#endif

#define SPIN_INIT(Q) spinlock_init(&(Q)->lock);
#define SPIN_LOCK(Q) spinlock_lock(&(Q)->lock);
#define SPIN_UNLOCK(Q) spinlock_unlock(&(Q)->lock);
#define SPIN_TRYLOCK(Q) spinlock_trylock(&(Q)->lock);

struct spinlock {
    atomic_flag_ lock;
};

static inline void
spinlock_init(struct spinlock* lock) {
    atomic_flag_ v = ATOMIC_FLAG_INIT_;
    lock->lock = v;
}

static inline void
spinlock_lock(struct spinlock* lock) {
    while (atomic_flag_test_and_set_(&lock->lock)) {}
}

static inline int
spinlock_trylock(struct spinlock* lock) {
    return atomic_flag_test_and_set_(&lock->lock) == 0;
}

static inline void
spinlock_unlock(struct spinlock* lock) {
    atomic_flag_clear_(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock* lock) {
    (void)lock;
}
#endif