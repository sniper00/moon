#ifndef _ATOMIC_H___
#define _ATOMIC_H___

#ifdef _MSC_VER
#include <Windows.h>

#define ATOM_CAS(ptr, oval, nval) (InterlockedCompareExchange((LONG volatile *)ptr, nval, oval) == oval)
#define ATOM_CAS_POINTER(ptr, oval, nval) (InterlockedCompareExchangePointer((PVOID volatile *)ptr, nval, oval) == oval)
#define ATOM_INC(ptr) (InterlockedIncrement64((LONG64 volatile *)ptr))
#define ATOM_FINC(ptr)  (InterlockedIncrement64((LONG64 volatile *)ptr)-1)
#define ATOM_DEC(ptr) (InterlockedDecrement64((LONG64 volatile *)ptr))
#define ATOM_ADD(ptr,n) (InterlockedAdd64((LONG64 volatile *)ptr, n))
#define ATOM_SUB(ptr,n) (InterlockedAdd64((LONG64 volatile *)ptr, -n))
#define ATOM_AND(ptr,n)  (InterlockedAnd64((LONG64 volatile *)ptr,n))
#else

#define ATOM_CAS(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)
#define ATOM_CAS_POINTER(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)
#define ATOM_INC(ptr) __sync_add_and_fetch(ptr, 1)
#define ATOM_FINC(ptr) __sync_fetch_and_add(ptr, 1)
#define ATOM_DEC(ptr) __sync_sub_and_fetch(ptr, 1)
#define ATOM_FDEC(ptr) __sync_fetch_and_sub(ptr, 1)
#define ATOM_ADD(ptr,n) __sync_add_and_fetch(ptr, n)
#define ATOM_SUB(ptr,n) __sync_sub_and_fetch(ptr, n)
#define ATOM_AND(ptr,n) __sync_and_and_fetch(ptr, n)

#endif

#endif