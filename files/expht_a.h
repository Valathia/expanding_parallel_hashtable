#include "config.h"

#ifdef MUTEX 
    typedef struct __attribute__ ((aligned(64))) node {
        size_t value;
        struct node* next;
    }node;

    typedef struct __attribute__ ((aligned(64))) node_bucket{
        LOCKS lock_b; 
        node* first; 
    }node_bucket;

    typedef struct hash_header {
        LOCKS lock;
        int64_t mode;
        int64_t n_ele;
        int64_t n_buckets;
    }hash_header;

    typedef struct __attribute__ ((aligned(64))) counter {
        int64_t ops;
        int64_t count;
        int64_t header; //n_buckets para identificar o header, não há 2 headers com o mesmo nr de buckets
        //char _align[CACHE_LINE-sizeof(int64_t)*3];
    }counter;

    typedef struct __attribute__ ((aligned(64))) access_header {
        LOCKS lock;
        int64_t thread_id;
        //char _align[CACHE_LINE-(sizeof(LOCKS))-sizeof(int64_t)];
        counter insert_count[MAXTHREADS];
    }access_header;
#else 
    typedef struct __attribute__ ((aligned(64))) node {
        size_t value;
        struct node* next;
    }node;

    typedef struct __attribute__ ((aligned(64))) node_bucket{
        LOCKS lock_b; 
        node* first; 
    }node_bucket;

    typedef struct __attribute__ ((aligned(64))) hash_header {
        LOCKS lock;
        int64_t mode;
        int64_t n_ele;
        int64_t n_buckets;
        //char _align[CACHE_LINE-sizeof(int64_t)*2];
    }hash_header;

    typedef struct __attribute__ ((aligned(64))) counter {
        int64_t ops;
        int64_t count;
        int64_t header; //n_buckets para identificar o header, não há 2 headers com o mesmo nr de buckets
        //char _align[CACHE_LINE-sizeof(int64_t)*3];
    }counter;

    typedef struct access_header {
        LOCKS lock;
        int64_t thread_id;
        counter insert_count[MAXTHREADS];
    }access_header;
#endif

typedef struct hashtable {
    hash_header header;
    node_bucket bucket;
}hashtable;

// struct to 
typedef struct __attribute__ ((aligned(64))) access {
    access_header header;
    hashtable* ht;
    //char _align[CACHE_LINE-sizeof(hashtable*)];
}access;

hashtable* create_table(int64_t s);
access* create_acess(int64_t s, int64_t n_threads);

int64_t main_hash(access* entry,size_t value, int64_t id_ptr);
int64_t search(access* entry,size_t value, int64_t id_ptr);
int64_t delete(access* entry,size_t value, int64_t id_ptr);
int64_t insert(hashtable* b, access* entry, node* n, int64_t id_ptr);
int64_t get_thread_id(access* entry);
void imprimir_hash(hashtable* ht);