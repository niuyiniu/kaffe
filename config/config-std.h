/*
 * config-std.h
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#ifndef __config_std_h
#define __config_std_h

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if defined(HAVE_STRING_H)
#include <string.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined (HAVE_TIME_H)
#include <time.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#if !defined(HAVE_WINDOWS_H) && defined(HAVE_WINNT_H)
#include <winnt.h>
#endif
#if defined(HAVE_WINTYPES_H)
#include <wintypes.h>
#endif
#if defined(HAVE_WTYPES_H)
#include <wtypes.h>
#endif
#if defined(HAVE_WINBASE_H)
#include <winbase.h>
#endif
#if defined(HAVE_BSD_LIBC_H)
#include <bsd/libc.h>
#endif
#if defined(HAVE_KERNEL_OS_H)
#include <kernel/OS.h>
#endif

#undef	__NORETURN__
#if defined(__GNUC__)
#define	__NORETURN__ __attribute__((__noreturn__))
#else
#define	__NORETURN__
#endif

#undef	__UNUSED__
#if defined(__GNUC__)
#define	__UNUSED__ __attribute__((__unused__))
#else
#define	__UNUSED__
#endif

/* SunOS has on_exit only */
#if !defined(HAVE_ATEXIT) && defined(HAVE_ON_EXIT)
#define atexit(x)       on_exit(x, 0)
#endif

#if defined(WITH_DMALLOC)
#  include <dmalloc.h>            
#endif

#endif
