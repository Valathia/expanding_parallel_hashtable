#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct pair {
    int key;
    int val;
}pair;

typedef struct node {
    pair data;
    struct node* next;
}node;

typedef struct node_bucket{
    pthread_rwlock_t lock_b;
    node* first;
}node_bucket;

typedef struct hash_header {
    pthread_rwlock_t lock;
    int n_ele;
    int n_buckets;
    //int tresh;
}hash_header;

typedef struct hashtable {
    hash_header header;
    node_bucket bucket;
}hashtable;


hashtable* create_table(int s);

int main_hash(hashtable* b,int k, int v, void* id_ptr, int* r_n_elem, int flag);
void imprimir_hash(hashtable* ht);