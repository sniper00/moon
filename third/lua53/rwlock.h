#ifndef _RWLOCK_H___
#define _RWLOCK_H___

#include "atomic.h"

/* read write lock */

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
			atom_sync();
		}
		ATOM_INC(&lock->read);
		if (lock->write) {
			ATOM_DEC(&lock->read);
		}
		else {
			break;
		}
	}
}

static inline void
rwlock_wlock(struct rwlock *lock) {
	atom_spinlock(&lock->write);
	while (lock->read) {
		atom_sync();
	}
}

static inline void
rwlock_wunlock(struct rwlock *lock) {
	atom_spinunlock(&lock->write);
}

static inline void
rwlock_runlock(struct rwlock *lock) {
	ATOM_DEC(&lock->read);
}

#endif