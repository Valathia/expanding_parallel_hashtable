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
#endif

#define TRESH 2

#define ARRAY_INIT_SIZE 8

#define CACHE_LINE 64

#define UPDATE 1000

#define MAXTHREADS 32

#ifdef MUTEX
    #define LOCKS pthread_mutex_t
    #define LOCK_INIT pthread_mutex_init
    #define UNLOCK pthread_mutex_unlock
    #define WRITE_LOCK pthread_mutex_lock
    #define READ_LOCK pthread_mutex_lock
    #define TRY_LOCK pthread_mutex_trylock
#else 
    #define LOCKS pthread_rwlock_t
    #define LOCK_INIT pthread_rwlock_init
    #define UNLOCK pthread_rwlock_unlock
    #define WRITE_LOCK pthread_rwlock_wrlock
    #define READ_LOCK pthread_rwlock_rdlock
    #define TRY_LOCK pthread_rwlock_trywrlock
#endif

#ifdef ARRAY 
    typedef struct array_bucket {
        LOCKS lock_b;
        size_t* array;
        int64_t n;
        int64_t size;
    }BUCKET;
#else 
    typedef struct __attribute__ ((aligned(64))) node {
        size_t value;
        struct node* next;
    }node;

    typedef struct __attribute__ ((aligned(64))) node_bucket{
        LOCKS lock_b; 
        node* first; 
    }BUCKET;
#endif



typedef struct hash_header {
    LOCKS lock;
    int64_t mode;
    int64_t n_ele;
    int64_t n_buckets;
}hash_header;

typedef struct __attribute__((aligned(64))) hashtable {
    hash_header header;
    BUCKET bucket;
}hashtable;

typedef struct __attribute__ ((aligned(64))) counter {
    int64_t ops;
    int64_t count;
    int64_t header; //n_buckets para identificar o header, não há 2 headers com o mesmo nr de buckets
    int64_t ht_header_lock_count;
    int64_t expansion_at_bucket;
}counter;

//spacing so hopefully the lock for the ht pointer is separated from the header
typedef struct access_header {
    LOCKS lock;
    int64_t thread_id;
    char _align[16];
    counter insert_count[MAXTHREADS+1];
}access_header;

// struct to 
typedef struct access {
    access_header header;
    LOCKS lock;
    hashtable* ht;
}access;

hashtable* create_table(int64_t s);
access* create_acess(int64_t s, int64_t n_threads);

int64_t main_hash(access* entry,size_t value, int64_t id_ptr);
int64_t search(access* entry,size_t value, int64_t id_ptr);
int64_t delete(access* entry,size_t value, int64_t id_ptr);
int64_t get_thread_id(access* entry);
void imprimir_hash(hashtable* ht, FILE* f);

#ifdef ARRAY
    int64_t insert(hashtable* b, access* entry, size_t value, int64_t id_ptr);
#else 
    int64_t insert(hashtable* b, access* entry, node* n, int64_t id_ptr);
    int64_t bucket_size(node *first, hashtable* b);
#endif