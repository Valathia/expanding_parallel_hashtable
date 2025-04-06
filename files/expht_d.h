#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define TRESH 4
#define UPDATE 1000


#ifdef __APPLE__
	#include <jemalloc/jemalloc.h>
#else 
	#include "../cenas_instaladas/include/jemalloc/jemalloc.h"
#endif


#ifdef MUTEX
    #define LOCKS pthread_mutex_t
    #define LOCK_INIT pthread_mutex_init
    #define UNLOCK pthread_mutex_unlock
    #define WRITE_LOCK pthread_mutex_lock
    #define READ_LOCK pthread_mutex_lock
//    #define VAL 1
#else 
    #define LOCKS pthread_rwlock_t
    #define LOCK_INIT pthread_rwlock_init
    #define UNLOCK pthread_rwlock_unlock
    #define WRITE_LOCK pthread_rwlock_wrlock
    #define READ_LOCK pthread_rwlock_rdlock
//    #define VAL 0
#endif


typedef struct pair {
    size_t key;
    size_t val;
}pair;

typedef struct node {
    pair data;
    struct node* next;
}node;

typedef struct node_bucket{
    LOCKS lock_b; 
    node* first; 
}node_bucket;

typedef struct hash_header {
    LOCKS lock;
    int64_t n_ele;
    int64_t n_buckets;
    //int tresh;
}hash_header;

typedef struct hashtable {
    hash_header header;
    node_bucket bucket;
}hashtable;


typedef struct access_header {
    int64_t thread_id;
    int64_t insert_count[64];
    LOCKS lock;
}access_header;

// struct to 
typedef struct access {
    access_header header;
    hashtable* ht;
}access;

hashtable* create_table(int64_t s);
access* create_acess(int64_t s, int64_t n_threads);


//int main_hash(hashtable* b,int k, int v, void* id_ptr, int* r_n_elem, int flag);
int64_t main_hash(access* entry,size_t value, int64_t id_ptr);
int64_t search(access* entry,size_t value, int64_t id_ptr);
int64_t delete(access* entry,size_t value, int64_t id_ptr);
int64_t insert(access* entry, node* n, int64_t id_ptr);
int64_t get_thread_id(access* entry);
void imprimir_hash(hashtable* ht);