#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <jemalloc/jemalloc.h>

#if !defined(TRESH) 
    #define TRESH 2
#endif 

#if !defined(RATIO)
    #define RATIO 1
#endif 

#define ARRAY_INIT_SIZE 4

#define CACHE_LINE 64

#define UPDATE 1000

#define MAXTHREADS 32


#define Hash(v,n) (v&(n-(size_t)1))                                 // x % n == x & n-1 --- só funciona porque os nr de buckets é uma potência de base 2
#define IsHashRef(f) ((uint64_t)f&1)                                //função que verifica se o bucket está a apontar para uma hastable nova é 1 se estiver a apontar para uma HT nova
#define Mask(ht) ((void*)(uint64_t*)((uint64_t)ht | (uint64_t)1))
#define Unmask(pt) ((void *)((uint64_t)pt & ~(uint64_t)(1<<0))) 
#define Cond1(chain) (chain>TRESH)
#define Cond2(elem,buckets) (elem>RATIO*buckets)

//isto está desactualizado e eventualmente pode ser removido para ser só o Mutex
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
    int64_t ops;                    // quantas operaçõesa de insert/delete a thread fez 
    int64_t count;                  // valor a somar ao Header da HT ao fim de 1000 ops. (+1 por Insert, -1 por Delete)
    int64_t header;                 // n_buckets para identificar o header, não há 2 headers com o mesmo nr de buckets
    int64_t all_ops;                // # total de operações que a thread faz 
}counter;

//spacing so hopefully the lock for the ht pointer is separated from the header
typedef struct support_header {
    LOCKS lock;
    int64_t thread_id;
    char _align[16];
    counter insert_count[MAXTHREADS+1];
}support_header;

// support struct to hold integration between the struct and bench
// has thread bookeeping structures
typedef struct support {
    support_header header;
    LOCKS lock;
    hashtable* ht;
}support;

hashtable* create_table(int64_t s);
support* create_acess(int64_t s, int64_t n_threads);

int64_t main_hash(support* entry,size_t value, int64_t id_ptr);
int64_t search(support* entry,size_t value, int64_t id_ptr);
int64_t delete(support* entry,size_t value, int64_t id_ptr);
int64_t get_thread_id(support* entry);
void reset_id_counter(support* entry);
void imprimir_hash(hashtable* ht, FILE* f);

#ifdef ARRAY
    int64_t insert(hashtable* b, support* entry, size_t value, int64_t id_ptr);
#else 
    int64_t insert(hashtable* b, support* entry, node* n, int64_t id_ptr);
    int64_t bucket_size(node *first, hashtable* b);
#endif
//---------------------------------------------------------------------------------------------------------------

// condição de expansão actual:
//  #nodes_total > RATIO*#buckets && #nodes_num_bucket > threshhold

//---------------------------------------------------------------------------------------------------------------
