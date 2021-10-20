#ifndef _RWLOCK_H___
#define _RWLOCK_H___

/* read write lock */

#if defined(_MSC_VER)
#include <synchapi.h>

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
#include "atomic.h"

struct rwlock {
	ATOM_INT write;
	ATOM_INT read;
};

static inline void
rwlock_init(struct rwlock* lock) {
	ATOM_INIT(&lock->write, 0);
	ATOM_INIT(&lock->read, 0);
}

static inline void
rwlock_rlock(struct rwlock* lock) {
	for (;;) {
		while (ATOM_LOAD(&lock->write)) {}
		ATOM_FINC(&lock->read);
		if (ATOM_LOAD(&lock->write)) {
			ATOM_FDEC(&lock->read);
		}
		else {
			break;
		}
	}
}

static inline void
rwlock_wlock(struct rwlock* lock) {
	ATOM_INT clear;
	ATOM_INIT(&clear, 0);
	while (!ATOM_CAS(&lock->write, clear, 1)) {}
	while (ATOM_LOAD(&lock->read)) {}
}

static inline void
rwlock_wunlock(struct rwlock* lock) {
	ATOM_STORE(&lock->write, 0);
}

static inline void
rwlock_runlock(struct rwlock* lock) {
	ATOM_FDEC(&lock->read);
}
#endif
#endif
