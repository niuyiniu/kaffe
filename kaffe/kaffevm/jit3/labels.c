/* labels.c
 * Manage the code labels and links.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "gtypes.h"
#include "classMethod.h"
#include "labels.h"
#include "code-analyse.h"
#include "itypes.h"
#include "seq.h"
#include "constpool.h"
#include "gc.h"
#include "md.h"
#include "support.h"
#include "machine.h"
#include "thread.h"
#include "jthread.h"

static label* firstLabel;
static label* lastLabel;
static label* currLabel;

static uint32 labelCount;

/* Custom edition */
#define	kprintf	kaffe_dprintf
#define	gc_calloc_fixed(A,B)	gc_malloc((A)*(B), GC_ALLOC_JITTEMP)
#include "debug.h"

#if defined(DEBUG)
char *getLabelName(label *l)
{
	static char labeladdress[32]; /* XXX one more global */
	
	char *retval;

	assert(l != 0);
	
	if( ((l->type & Ltomask) == Lcode) && (l->to < pc) )
	{
		/*
		 * The code has already been generated, just use its offset
		 * instead of the symbolic name.
		 */
		sprintf(labeladdress, "0x%x", INSNPC(l->to));
		retval = labeladdress;
	}
	else
	{
		retval = l->name;
	}
	return( retval );
}
#endif

void
resetLabels(void)
{
	currLabel = firstLabel;
}

label *getLastEpilogueLabel(void)
{
	label* l, *retval = 0;

	for (l = firstLabel; l != currLabel; l = l->next) {
		if ((l->type & Ltomask) == Lepilogue) {
			retval = l;
			/*
			 * Keep going until we've hit the end of the active
			 * set of labels.
			 */
		}
	}
DBG(MOREJIT, if( retval ) { kprintf("%s:\n", retval->name); });
	return( retval );
}

/*
 * Set epilogue label destination.
 */
void
setEpilogueLabel(uintp to)
{
	label* l;

	for (l = firstLabel; l != currLabel; l = l->next) {
		if ((l->type & Ltomask) == Lepilogue) {
DBG(MOREJIT, kprintf("%s:\n", l->name));
			/* Change type to internal. */
			l->type = Linternal | (l->type & ~Ltomask);
			l->to = to;
			/* Keep going until all returns have been updated. */
		}
	}
}


/*
 * Link labels.
 * This function uses the saved label information to link the new code
 * fragment into the program.
 */
void
linkLabels(uintp codebase)
{
	long dest;
	int* place;
	label* l;

	assert(codebase != 0);
	
	for (l = firstLabel; l != currLabel; l = l->next) {

		/* Ignore this label if it hasn't been used */
		if (l->type == Lnull) {
			continue;
		}

		/* Find destination of label */
		switch (l->type & Ltomask) {
		case Lexternal:
			dest = l->to;	/* External code reference */
			break;
		case Lconstant:		/* Constant reference */
			dest = ((constpool*)l->to)->at;
			break;
		case Linternal:		/* Internal code reference */
		/* Lepilogue is changed to Linternal in setEpilogueLabel() */
			dest = l->to + codebase;
			break;
		case Lcode:		/* Reference to a bytecode */
			assert(INSNPC(l->to) != (uintp)-1);
			dest = INSNPC(l->to) + codebase;
			break;
		case Lgeneral:		/* Dest not used */
			dest = 0;
			break;
		default:
			goto unhandled;
		}

		/*
		 * Adjust the destination based on the type of reference and
		 * possibly its source.
		 */
		switch (l->type & Lfrommask) {
		case Labsolute:		/* Absolute address */
			break;
		case Lrelative:		/* Relative address */
			dest -= l->from + codebase;
			break;
		case Lfuncrelative:	/* Relative to function */
			dest -= codebase;
			break;
		default:
			goto unhandled;
		}

		/* Get the insertion point. */
		switch (l->type & Latmask) {
		case Lconstantpool:
			{
				constpool *cp;

				cp = (constpool *)l->at;
				place = (int *)cp->at;

				assert(cp->type == CPlabel);
			}
			break;
		default:
			place = (int*)(l->at + codebase);
			break;
		}

		switch (l->type & Ltypemask) {
		case Lquad:
			*(uint64*)place = dest;
			break;
		case Llong:
			*(uint32*)place = dest;
			break;

		/* Machine specific labels go in this magic macro */
			EXTRA_LABELS(place, dest, l);

		unhandled:
#if 0
		default:
#if defined(DEBUG)
			kprintf("Label type 0x%x not supported (%p).\n", l->type & Ltypemask, l);
#endif
			ABORT();
#endif
		}
#if 0
		/*
		 * If we were saving relocation information we must save all
		 * labels which are 'Labsolute', that is they hold an absolute
		 * address for something.  Note that this doesn't catch
		 * everything, specifically it doesn't catch string objects
		 * or references to classes.
		 */
		if ((l->type & Labsolute) != 0) {
			l->snext = savedLabel;
			savedLabel = l;
		}
#endif
	}
}

label*
newLabel(void)
{
	int i;
	label* ret;

	ret = currLabel;
	if (ret == 0) {
		/* Allocate chunk of label elements */
		ret = gc_calloc_fixed(ALLOCLABELNR, sizeof(label));

		/* Attach to current chain */
		if (lastLabel == 0) {
			firstLabel = ret;
		}
		else {
			lastLabel->next = ret;
		}
		lastLabel = &ret[ALLOCLABELNR-1];

		/* Link elements into list */
		for (i = 0; i < ALLOCLABELNR-1; i++) {
#if defined(DEBUG)
			sprintf(ret[i].name, "L%d", labelCount + i);
#endif
			ret[i].next = &ret[i+1];
		}
		ret[ALLOCLABELNR-1].next = 0;
	}
	currLabel = ret->next;
	labelCount += 1;
	return (ret);
}

label*
getInternalLabel(label **lptr, uintp pc)
{
	label *curr, *retval = 0;

	assert(lptr != 0);
	
	if( *lptr == 0 )
	{
		/* Start at the head of the list. */
		*lptr = firstLabel;
	}
	curr = *lptr;
	while( curr && (curr != currLabel) && !retval )
	{
		switch( curr->type & Ltomask )
		{
		case Linternal:
			if( curr->to == pc )
			{
				*lptr = curr->next;
				retval = curr;
			}
			break;
		case Lcode:
			if( INSNPC(curr->to) == pc )
			{
				*lptr = curr->next;
				retval = curr;
			}
			break;
		}
		curr = curr->next;
	}
	return( retval );
}
