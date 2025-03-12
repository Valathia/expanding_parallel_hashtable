#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef MUTEX
    #define LOCKS pthread_mutex_t
    #define LOCK_INIT pthread_mutex_init
    #define UNLOCK pthread_mutex_unlock
    #define WRITE_LOCK pthread_mutex_lock
    #define READ_LOCK pthread_mutex_lock
    //#define VAL 1
#else 
    #define LOCKS pthread_rwlock_t
    #define LOCK_INIT pthread_rwlock_init
    #define UNLOCK pthread_rwlock_unlock
    #define WRITE_LOCK pthread_rwlock_wrlock
    #define READ_LOCK pthread_rwlock_rdlock
    //#define VAL 0
#endif


typedef struct pair {
    int key;
    int val;
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
    int n_ele;
    int n_buckets;
    //int tresh;
}hash_header;

typedef struct hashtable {
    hash_header header;
    node_bucket bucket;
}hashtable;

typedef struct access {
    hashtable* ht;
    int thread_count;
}access;

hashtable* create_table(int s);
access* create_acess(int s);
//

//int main_hash(hashtable* b,int k, int v, void* id_ptr, int* r_n_elem, int flag);
int main_hash(access* entry,int value, void* id_ptr);
int search(hashtable* b,int value, void* id_ptr);
int delete(hashtable* b,int value, void* id_ptr);
int insert(hashtable* b, node* n, void* id_ptr);
void imprimir_hash(hashtable* ht);