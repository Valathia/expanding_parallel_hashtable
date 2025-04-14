#include "config.h"

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
    int64_t mode;
    int64_t n_ele;
    int64_t n_buckets;
}hash_header;

typedef struct hashtable {
    hash_header header;
    node_bucket bucket;
}hashtable;

typedef struct counter {
    int64_t ops;
    int64_t count;
    int64_t header; //n_buckets para identificar o header, não há 2 headers com o mesmo nr de buckets
}counter;

typedef struct access_header {
    int64_t thread_id;
    counter insert_count[64];
    LOCKS lock;
}access_header;

// struct to 
typedef struct access {
    access_header header;
    hashtable* ht;
    char _align[CACHE_LINE-sizeof(hashtable*)];
}access;

hashtable* create_table(int64_t s);
access* create_acess(int64_t s, int64_t n_threads);


//int main_hash(hashtable* b,int k, int v, void* id_ptr, int* r_n_elem, int flag);
int64_t main_hash(access* entry,size_t value, int64_t id_ptr);
int64_t search(access* entry,size_t value, int64_t id_ptr);
int64_t delete(access* entry,size_t value, int64_t id_ptr);
int64_t insert(hashtable* b, access* entry, node* n, int64_t id_ptr);
int64_t get_thread_id(access* entry);
void imprimir_hash(hashtable* ht);