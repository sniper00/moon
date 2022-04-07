#ifndef  _SPINLOCK_H__
#define _SPINLOCK_H__

#include "atomic.h"

#define atomic_test_and_set_(ptr) STD_ atomic_exchange_explicit(ptr, 1, STD_ memory_order_acquire)
#define atomic_clear_(ptr) STD_ atomic_store_explicit(ptr, 0, STD_ memory_order_release);
#define atomic_load_relaxed_(ptr) STD_ atomic_load_explicit(ptr, STD_ memory_order_relaxed)

#if (defined(__x86_64__)  || defined(_WIN64))
#include <immintrin.h>
#define atomic_pause_() _mm_pause()
#else
#define atomic_pause_() ((void)0)
#endif


#define SPIN_INIT(Q) spinlock_init(&(Q)->lock);
#define SPIN_LOCK(Q) spinlock_lock(&(Q)->lock);
#define SPIN_UNLOCK(Q) spinlock_unlock(&(Q)->lock);
#define SPIN_TRYLOCK(Q) spinlock_trylock(&(Q)->lock);

struct spinlock {
	STD_ atomic_int lock;
};

static inline void
spinlock_init(struct spinlock *lock) {
	STD_ atomic_init(&lock->lock, 0);
}

static inline void
spinlock_lock(struct spinlock *lock) {
	for (;;) {
	  if (!atomic_test_and_set_(&lock->lock))
		return;
	  while (atomic_load_relaxed_(&lock->lock))
		atomic_pause_();
	}
}

static inline int
spinlock_trylock(struct spinlock *lock) {
	return !atomic_load_relaxed_(&lock->lock) &&
		!atomic_test_and_set_(&lock->lock);
}

static inline void
spinlock_unlock(struct spinlock *lock) {
	atomic_clear_(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock *lock) {
	(void) lock;
}

#endif