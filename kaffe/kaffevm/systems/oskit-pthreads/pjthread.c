/*
 * Copyright (c) 1998 The University of Utah. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file.
 *
 * Contributed by the Flux Research Group at the University of Utah.
 * Authors: Godmar Back, Leigh Stoller
 */

#include "debug.h"
#include "jthread.h"
#include <sys/wait.h>
#include <sys/time.h>
#include <oskit/dev/dev.h>

/* thread status */
#define THREAD_NEWBORN                	0
#define THREAD_RUNNING                  1
#define THREAD_DYING                    2
#define THREAD_DEAD                     3

/*
 * Variables.
 * These should be kept static to ensure encapsulation.
 */
static int talive; 		/* number of threads alive */
static int tdaemon;		/* number of daemons alive */
static void (*runOnExit)(void);	/* function to run when all non-daemon die */

static struct jthread* liveThreads;	/* list of all live threads */

static jmutex threadLock;	/* static lock to protect liveThreads etc. */
static void remove_thread(jthread_t tid);
static void deathcallback(void *arg);

/*
 * the following variables are set by jthread_init, and show how the
 * threading system is parametrized.
 */
static void *(*allocator)(size_t); 	/* malloc */
static void (*deallocator)(void*);	/* free */
static void (*destructor1)(void*);	/* call when a thread exits */
static void (*onstop)(void);		/* call when a thread is stopped */
static char *(*nameThread)(void *); 	/* call to get a thread's name */
static int  max_priority;		/* maximum supported priority */
static int  min_priority;		/* minimum supported priority */

pthread_key_t	cookie_key;	/* key to map pthread -> Hjava_lang_Thread */
pthread_key_t	jthread_key;	/* key to map pthread -> jthread */

/*
 * Function declarations.
 */


/*============================================================================
 *
 * Functions related to interrupt handling
 *
 */

/*
 * disable interrupts
 */
void 
intsDisable(void)
{
	osenv_intr_disable();	/* XXX */
}

/*
 * restore interrupts
 */
void
intsRestore(void)
{
	osenv_intr_enable();	/* XXX */
}

/*
 * reenable interrupts, non-recursive version.
 */
void
intsRestoreAll(void)
{
	osenv_intr_enable();	/* XXX */
}

/*============================================================================
 *
 * Functions dealing with thread contexts and the garbage collection interface
 *
 */

/*
 * free a thread context
 */
void    
jthread_destroy(jthread_t tid)
{
	void *status;
	int oldstate;

	assert(tid);
DBG(JTHREAD, 
	dprintf("destroying tid %d\n", tid->native_thread);	
    )
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	pthread_join(tid->native_thread, &status);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	deallocator(tid);
}

/*
 * iterate over all live threads
 */
void
jthread_walkLiveThreads(void (*func)(void *jlThread))
{
        jthread_t tid;

	jmutex_lock(&threadLock);
        for (tid = liveThreads; tid != NULL; tid = tid->nextlive) {
                func(tid->jlThread);
        }
	jmutex_unlock(&threadLock);
}

/*
 * determine the interesting stack range for a conservative gc
 */
void
jthread_extract_stack(jthread_t jtid, void **from, unsigned *len)
{
	struct pthread_state ps;

	if (pthread_getstate(jtid->native_thread, &ps))
		panic("jthread_extract_stack: tid(%d)", jtid->native_thread);
	
#if defined(STACK_GROWS_UP)
#error FIXME
#else
#if notyet
	*from = (void *)ps.stackptr;
	*len = ps.stackbase + ps.stacksize - ps.stackptr;
#else
	*from = (void *)ps.stackptr;
	*len = ps.stackbase - ps.stackptr;	/* base is top XXX */

	if (*len < 0 || *len > (256*1024)) {
	    panic("(%d) pthread_getstate(%d) reported obscene numbers: "
		  "base = 0x%x, sp = 0x%x\n", 
		  pthread_self(),
		  jtid->native_thread,
		  ps.stackbase, ps.stackptr);
	    exit(-1);
	}
#endif
#endif
DBG(JTHREAD,
	dprintf("extract_stack(%) base=%p size=%d sp=%p; from=%p len=%d\n", 
		jtid->native_thread,
		ps.stackbase, ps.stacksize, ps.stackptr, *from, *len);
    )
}

/*
 * determine whether an address lies on your current stack frame
 */
int
jthread_on_current_stack(void *bp)
{
	struct pthread_state ps;
	int rc;

	if (pthread_getstate(pthread_self(), &ps))
		panic("jthread_on_current_stack: pthread_getstate(%d)",
		      pthread_self());
#if notyet
        rc = (uint32)bp >= ps.stackbase && 
	     (uint32)bp < ps.stackbase + ps.stacksize;
#else
	rc = (uint32)bp < ps.stackbase;		/* base is top XXX */
#endif
DBG(JTHREAD,
	dprintf("on current stack(%d) base=%p size=%d bp=%p %s\n",
		pthread_self(),
		ps.stackbase, ps.stacksize, bp, (rc ? "yes" : "no"));
    )
	return rc;
}       

/*
 * See if there is enough room on the stack.
 */
int
jthread_stackcheck(int need)
{
	struct pthread_state ps;
	int room;

	if (pthread_getstate(pthread_self(), &ps))
		panic("jthread_stackcheck: pthread_getstate(%d)",
		      pthread_self());
	
#if defined(STACK_GROWS_UP)
#	error FIXME
#else
	room = ps.stacksize - (ps.stackbase - ps.stackptr);
#endif
	
DBG(JTHREAD,
	dprintf("stackcheck(%d) need=%d base=%p size=%d sp=%p room=%d\n",
		pthread_self(),
		need, ps.stackbase, ps.stacksize, ps.stackptr, room);
    )
	return (room >= need);
}       

/* 
 * XXX this is supposed to count the number of stack frames 
 */
int
jthread_frames(jthread_t thrd)
{
        return 0;
}

/*============================================================================
 *
 * Functions for initialization and thread creation
 *
 */

/*
 * Initialize the threading system.
 *
 * XXX: pthread_init() has already been called.
 */
jthread_t 
jthread_init(int pre,
	int maxpr, int minpr, int mainthreadpr, 
	size_t mainThreadStackSize,
	void *(*_allocator)(size_t), 
	void (*_deallocator)(void*),
	void (*_destructor1)(void*),
	void (*_onstop)(void),
	char *(*_nameThread)(void *tid))
{
	pthread_t	pmain;
	jthread_t	jtid;

	max_priority = maxpr;
	min_priority = minpr;
	allocator = _allocator;
	deallocator = _deallocator;
	onstop = _onstop;
	nameThread = _nameThread;
	destructor1 = _destructor1;

	pmain = pthread_self();

        /*
         * XXX: ignore mapping of min/max priority for now and assume
         * pthread priorities include java priorities
         */
        pthread_setprio(pmain, mainthreadpr);

        pthread_key_create(&jthread_key, 0 /* destructor */);
        pthread_key_create(&cookie_key, 0 /* destructor */);

        jtid = allocator(sizeof (*jtid));
        SET_JTHREAD(jtid);

	jtid->native_thread = pmain;
	pthread_cleanup_push(deathcallback, jtid);

        jtid->nextlive = liveThreads;
        liveThreads = jtid;
	jtid->status = THREAD_RUNNING;
	jmutex_initialise(&threadLock);
        talive++;
DBG(JTHREAD,
	dprintf("main thread has id %d\n", jtid->native_thread);
    )
	return jtid;
}

/*
 * set a function to be run when all non-daemon threads have exited
 */
void 
jthread_atexit(void (*f)(void))
{
	runOnExit = f;
}

/*
 * disallow cancellation
 */
void 
jthread_disable_stop(void)
{
#if defined(PTHREAD_CANCEL_DISABLE)
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
#endif
}

/*
 * reallow cancellation and stop if cancellation pending
 */
void 
jthread_enable_stop(void)
{
#if defined(PTHREAD_CANCEL_ENABLE)
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
#endif
}

/*
 * interrupt a thread
 */
void
jthread_interrupt(jthread_t tid)
{
    panic("jthread_interrupt");
    
}

/*
 * cleanup handler for a given thread.  This handler is called when that
 * thread is killed.
 */
static void
deathcallback(void *arg)
{
	jthread_t tid = (jthread_t)arg;
	tid->status = THREAD_DYING;	
	onstop();
	remove_thread(tid);
	/* by returning, we proceed with pthread_exit */
}

/*
 * start function for each thread.  
 * This function install the cleanup handler, sets jthread-specific 
 * data and calls the actual work function.
 */
void
start_me_up(void *arg)
{
	jthread_t tid = (jthread_t)arg;

DBG(JTHREAD, 
	dprintf("starting thread %d\n", tid->native_thread); )
	jmutex_lock(&threadLock);
	pthread_cleanup_push(deathcallback, tid);
	SET_COOKIE(tid->jlThread);
	SET_JTHREAD(tid);
	jmutex_unlock(&threadLock);

        tid->status = THREAD_RUNNING;
DBG(JTHREAD, 
	dprintf("calling thread %d\n", tid->native_thread); )
	tid->func(tid->jlThread);
DBG(JTHREAD, 
	dprintf("thread %d returned, calling jthread_exit\n", 
		tid->native_thread); 
    )
	jthread_exit(); 
}

/*
 * create a new jthread
 */
jthread_t
jthread_create(unsigned int pri, void (*func)(void *), int daemon,
        void *jlThread, size_t threadStackSize)
{
	int err;
	pthread_t new;
	jthread_t tid;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, threadStackSize);
	pthread_attr_setprio(&attr, pri);

	/* 
	 * Note that we create the thread in a joinable state, which is the
	 * default.  Our finalizer will join the threads, allowing the
	 * pthread system to free its resources.
	 */

        tid = allocator(sizeof (*tid));
        assert(tid != 0);      /* XXX */

	jmutex_lock(&threadLock);
        tid->jlThread = jlThread;
        tid->func = func;

        tid->nextlive = liveThreads;
        liveThreads = tid;
        tid->status = THREAD_NEWBORN;
	jmutex_unlock(&threadLock);

	err = pthread_create(&new, &attr, start_me_up, tid);

	tid->native_thread = new;

        talive++;       
        if ((tid->daemon = daemon) != 0) {
                tdaemon++;
        }
DBG(JTHREAD,
	dprintf("created thread %d, daemon=%d\n", new, daemon); )

        return tid;
}

/*============================================================================
 *
 * Functions that are part of the user interface
 *
 */

/*      
 * sleep for time milliseconds
 */     
void
jthread_sleep(jlong time)
{
	pthread_sleep((oskit_s64_t)time);
}

/* 
 * Check whether a thread is alive.
 *
 * Note that threads executing their cleanup function are not (jthread-) alive.
 * (they're set to THREAD_DEAD)
 */
int
jthread_alive(jthread_t tid)
{
	return tid && (tid->status == THREAD_NEWBORN || 
		       tid->status == THREAD_RUNNING);
}

/*
 * Change thread priority.
 */
void
jthread_setpriority(jthread_t jtid, int prio)
{
	pthread_setprio(jtid->native_thread, prio);
}

/*
 * Stop a thread in its tracks.
 */
void
jthread_stop(jthread_t jtid)
{
	/* can I cancel myself safely??? */
#if defined(PTHREAD_CANCEL_ENABLE)
	pthread_cancel(jtid->native_thread);
#endif
}

static void
remove_thread(jthread_t tid)
{
	jthread_t* ntid;
	jmutex_lock(&threadLock);

	talive--;
	if (tid->daemon) {
		tdaemon--;
	}

	/* Remove thread from live list so it can be garbage collected */
	for (ntid = &liveThreads; *ntid != 0; ntid = &(*ntid)->nextlive) 
	{
		if (tid == (*ntid)) {
			(*ntid) = tid->nextlive;
			break;
		}
	}

	jmutex_unlock(&threadLock);

	/* If we only have daemons left, then we should exit. */
	if (talive == tdaemon) {
DBG(JTHREAD,
		dprintf("all done, closing shop\n");
    )
		if (runOnExit != 0) {
		    runOnExit();
		}

		/* does that really make sense??? */
		for (tid = liveThreads; tid != 0; tid = tid->nextlive) {
			if (destructor1)
				(*destructor1)(tid->jlThread);
			pthread_cancel(tid->native_thread);
		}

		/* Am I suppose to close things down nicely ?? */
		EXIT(0);
	} else {
		if (destructor1)
			(*destructor1)(tid->jlThread);
	}
}

/*
 * Have a thread exit.
 */
void
jthread_exit(void)
{
	jthread_t currentJThread = GET_JTHREAD();

DBG(JTHREAD,
	dprintf("jthread_exit called by %d\n", currentJThread->native_thread);
    )

	/* drop onstop handler if that thread exited by itself */
	assert (currentJThread->status != THREAD_DYING);
	pthread_cleanup_pop(0);

	assert (currentJThread->status != THREAD_DEAD);
	currentJThread->status = THREAD_DEAD;

	remove_thread(currentJThread);

	pthread_exit(0);
	while (1)
		assert(!"This better not return.");
}

/*
 * have main thread wait for all threads to finish
 */
void jthread_exit_when_done(void)
{
        while (talive > 1)
		jthread_yield();
	jthread_exit();
}

/*============================================================================
 * 
 * locking subsystem
 *
 */

void 
jmutex_initialise(jmutex *lock)
{
	pthread_mutex_init(lock, (const pthread_mutexattr_t *)0);
}

void
jmutex_lock(jmutex *lock)
{
	pthread_mutex_lock(lock);
}

void
jmutex_unlock(jmutex *lock)
{
	pthread_mutex_unlock(lock);
}

void
jcondvar_initialise(jcondvar *cv)
{
	pthread_cond_init(cv, (const pthread_condattr_t *)0);
}

void
jcondvar_wait(jcondvar *cv, jmutex *lock, jlong timeout)
{
	struct oskit_timespec abstime;		/* XXX */
	struct timeval now;

	if (timeout == (jlong)0) {
		pthread_cond_wait(cv, lock);
		return;
	}

	/* Need to convert timeout to an abstime. Very dumb! */
	gettimeofday(&now, 0);
	TIMEVAL_TO_TIMESPEC(&now, &abstime);

	abstime.tv_sec  += timeout / 1000;
	abstime.tv_nsec += (timeout % 1000) * 1000000;
	if (abstime.tv_nsec > 1000000000) {
		abstime.tv_sec  += 1;
		abstime.tv_nsec -= 1000000000;
	}
	pthread_cond_timedwait(cv, lock, &abstime);
}

void
jcondvar_signal(jcondvar *cv, jmutex *lock)
{
	pthread_cond_signal(cv);
}

void
jcondvar_broadcast(jcondvar *cv, jmutex *lock)
{
	pthread_cond_broadcast(cv);
}
