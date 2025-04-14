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
    int64_t n_ele;
    int64_t n_buckets;
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

int64_t main_hash(access* entry,size_t value, int64_t id_ptr);
int64_t search(access* entry,size_t value, int64_t id_ptr);
int64_t delete(access* entry,size_t value, int64_t id_ptr);
int64_t insert(hashtable* b, access* entry, node* n, int64_t id_ptr);
int64_t get_thread_id(access* entry);
void imprimir_hash(hashtable* ht);