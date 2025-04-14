#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __APPLE__
	#include "../../../../opt/homebrew/Cellar/jemalloc/5.3.0/include/jemalloc/jemalloc.h"
#else 
	#include "../cenas_instaladas/include/jemalloc/jemalloc.h"
    //leap
    //#include "/usr/include/jemalloc/jemalloc.h"
#endif

#define TRESH 4

#define CACHE_LINE 64

#define UPDATE 1000

#define MAXTHREADS 64

#ifdef MUTEX
    #define LOCKS pthread_mutex_t
    #define LOCK_INIT pthread_mutex_init
    #define UNLOCK pthread_mutex_unlock
    #define WRITE_LOCK pthread_mutex_lock
    #define READ_LOCK pthread_mutex_lock
   // #define VAL 1
#else 
    #define LOCKS pthread_rwlock_t
    #define LOCK_INIT pthread_rwlock_init
    #define UNLOCK pthread_rwlock_unlock
    #define WRITE_LOCK pthread_rwlock_wrlock
    #define READ_LOCK pthread_rwlock_rdlock
    // #define VAL 0
#endif