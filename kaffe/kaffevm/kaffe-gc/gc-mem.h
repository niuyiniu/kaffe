/* gc-mem.h
 * The heap manager.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */
/*
 * NB: this file contains only types and declaration relating to heap
 * management.  There's nothing relating to gc in here.
 */
#ifndef __gc_mem_h
#define	__gc_mem_h

#include "md.h"

#if !defined(ALIGNMENT_OF_SIZE)
#define ALIGNMENT_OF_SIZE(S)	(S)
#endif

#ifndef gc_pgsize
extern size_t gc_pgsize;
extern int gc_pgbits;
#endif

extern size_t gc_heap_total;
extern size_t gc_heap_allocation_size;
extern size_t gc_heap_limit;

#ifdef KAFFE_VMDEBUG
extern int gc_system_alloc_cnt;
#endif

/* ------------------------------------------------------------------------ */

typedef struct _gc_freeobj {
	struct _gc_freeobj*	next;
} gc_freeobj;

/* ------------------------------------------------------------------------ */

#define	MIN_OBJECT_SIZE		8
#define	MAX_SMALL_OBJECT_SIZE	8192
#define	NR_FREELISTS		20
#define	KGC_SMALL_OBJECT(S)	((S) <= max_small_object_size)

/**
 * Alignment for gc_blocks
 *
 */
#define	MEMALIGN		(ALIGNMENT_OF_SIZE(sizeof(jdouble)))

/**
 * rounds @V up to the next MEMALIGN boundary. 
 *
 */
#define	ROUNDUPALIGN(V)		(((uintp)(V) + MEMALIGN - 1) & -MEMALIGN)

/**
 * rounds @V down to the previous MEMALIGN boundary.
 *
 */
#define	ROUNDDOWNALIGN(V)	((uintp)(V) & -MEMALIGN)

/**
 * rounds @V up to the next page size.
 *
 */
#define	ROUNDUPPAGESIZE(V)	(((uintp)(V) + gc_pgsize - 1) & -gc_pgsize)

/* ------------------------------------------------------------------------ */

extern void	gc_heap_initialise (void);
extern void*	gc_heap_malloc(size_t);    
extern void	gc_heap_free(void*);

extern void*	gc_heap_grow(size_t);

/**
 * Evaluates to the size of the object that contains address @M.
 *
 */
#define	KGC_OBJECT_SIZE(M)	GCMEM2BLOCK(M)->size

/**
 * One block of the heap managed by kaffe's gc.
 *
 * It is basically one page of objects that are of the same size. 
 */
typedef struct _gc_block {
#ifdef KAFFE_VMDEBUG
	uint32			magic;	/* Magic number */
#endif
	struct _gc_freeobj*	free;	/* Next free sub-block */
	struct _gc_block*	next;	/* Next block in prim/small freelist */
	struct _gc_block*	pnext;	/* next primitive block */
	struct _gc_block*	pprev;	/* previous primitive block */
	uint32			size;	/* Size of objects in this block */
	uint16			nr;	/* Nr of objects in block */
	uint16			avail;	/* Nr of objects available in block */
	uint8*			funcs;	/* Function for objects */
	uint8*			state;	/* Colour & state of objects */
	uint8*			data;	/* Address of first object in */
} gc_block;

extern gc_block	* gc_primitive_reserve(void);
extern void	gc_primitive_free(gc_block* mem);

/* ------------------------------------------------------------------------ */

#define KGC_BLOCKS		((gc_block *) gc_block_base)

/**
 * Tests whether a block is in use.
 *
 */
#define GCBLOCKINUSE(B)		((B)->nr > 0)

/**
 * Evaluates to the array that contains the states of the objects contained in @B.
 *
 */
#define	GCBLOCK2STATE(B, N)	(&(B)->state[(N)])

/**
 * Evaluates to a gc_unit* of the @Nth object stored in gc_block @B
 *
 */
#define	GCBLOCK2MEM(B, N)	((gc_unit*)(&(B)->data[(N)*(B)->size]))

/**
 * Evaluates to a gc_freeobj* of the @Nth object stored in gc_block @B.
 *
 */
#define	GCBLOCK2FREE(B, N)	((gc_freeobj*)GCBLOCK2MEM(B, N))

/**
 * Evaluates to the size of the objects stored in gc_block @B
 *
 */
#define	GCBLOCKSIZE(B)		(B)->size

/** 
 * Evaluates to a gc_freeobj* equal to address @M. 
 *
 */
#define	GCMEM2FREE(M)		((gc_freeobj*)(M))

/**
 * Evaluates to the index of the object in gc_block @B that contains address @M.
 *
 */
#define	GCMEM2IDX(B, M)		(((uint8*)(M) - (B)->data) / (B)->size)

/**
 * Evaluates to the gc_block that contains address @M.
 *
 */
#define GCMEM2BLOCK(M)		(KGC_BLOCKS + ((((uintp) M) - gc_heap_base) \
					      >> gc_pgbits))

/**
 * Evaluates to the first usable address in gc_block @B.
 *
 */ 
#define GCBLOCK2BASE(B)		(((char *)gc_heap_base) \
					 + gc_pgsize * ((B) - KGC_BLOCKS))

#define ASSERT_ONBLOCK(OBJ, BLK) assert(GCMEM2BLOCK(OBJ) == BLK)

#endif