#ifndef _ATOMIC_H___
#define _ATOMIC_H___

#ifdef _MSC_VER

#include <windows.h>
#define inline __inline

#define ATOM_CAS(ptr, oval, nval) (InterlockedCompareExchange((LONG volatile *)ptr, nval, oval) == oval)
#define ATOM_CAS_POINTER(ptr, oval, nval) (InterlockedCompareExchangePointer((PVOID volatile *)ptr, nval, oval) == oval)
#define ATOM_INC(ptr) InterlockedIncrement((LONG volatile *)ptr)
#define ATOM_DEC(ptr) InterlockedDecrement((LONG volatile *)ptr)
#define ATOM_ADD(ptr,n) InterlockedExchangeAdd((LONG volatile *)ptr, n)
#define ATOM_SUB(ptr,n) InterlockedExchangeAdd((LONG volatile *)ptr, -n)
#define ATOM_AND(ptr,n)  InterlockedExchange((LONG volatile *)ptr,n)
#define atom_sync() MemoryBarrier()
#define atom_spinlock(ptr) while (InterlockedExchange((LONG volatile *)ptr , 1)) {};
#define atom_spintrylock(ptr) (InterlockedExchange((LONG volatile *)ptr , 1) == 0)
#define atom_spinunlock(ptr) InterlockedExchange((LONG volatile *)ptr, 0);

struct spinlock {
	LONG lock;
};

#else

#define ATOM_CAS(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)
#define ATOM_CAS_POINTER(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)
#define ATOM_INC(ptr) __sync_add_and_fetch(ptr, 1)
#define ATOM_DEC(ptr) __sync_sub_and_fetch(ptr, 1)
#define ATOM_ADD(ptr,n) __sync_add_and_fetch(ptr, n)
#define ATOM_SUB(ptr,n) __sync_sub_and_fetch(ptr, n)
#define ATOM_AND(ptr,n) __sync_and_and_fetch(ptr, n)

#define atom_sync() __sync_synchronize()
#define atom_spinlock(ptr) while (__sync_lock_test_and_set(ptr,1)) {}
#define atom_spintrylock(ptr) (__sync_lock_test_and_set(ptr,1) == 0)
#define atom_spinunlock(ptr) __sync_lock_release(ptr)

struct spinlock {
	int lock;
};
#endif

#endif