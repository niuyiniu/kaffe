/*
 * i386/univel_svr4/jit-md.h
 * Unixware i386 JIT configuration information.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#ifndef __i386_univel_svr4_jit_md_h
#define __i386_univel_svr4_jit_md_h

/**/
/* Include common information. */
/**/
#include "i386/jit.h"

/* Function prototype for signal handlers */
#define	EXCEPTIONPROTO SIGNAL_ARGS(sig, ctx)

/* Get the first exception frame from a signal handler */
#define	EXCEPTIONFRAME(f, c)						\
	(f).retbp = (c)->uc_mcontext.gregs[EBP];			\
	(f).retpc = (c)->uc_mcontext.gregs[EIP] + 1

#endif
