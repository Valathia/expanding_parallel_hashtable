#define ARRAY 1
#include "config.h"

//função que verifica se o bucket está a apontar para uma hastable nova 
size_t is_bucket_array(size_t* first) {
    return (uint64_t)first&1;
}

//imprime os nodes 
int imprimir_array(hashtable* b, int64_t index){

    for(int i=0; i<(&b->bucket)[index].n; i++) {
        if(i < (&b->bucket)[index].n-1) {
            printf("%lld , ",(&b->bucket)[index].array[i]);
        }
        else {
            printf("%lld \n",(&b->bucket)[index].array[i]);
        }
    }
    return (&b->bucket)[index].n;
}

void imprimir_hash(hashtable* ht) {
    int64_t real_num = 0;
    printf("-------- HASHTABLE %lld --------\n",ht->header.n_buckets);
    for(int64_t i=0;i<ht->header.n_buckets;i++) {
        
        printf("index : %lld\n",i);
        printf("---------------------------\n");

        real_num += imprimir_array(ht,i);
        
        printf("---------------------------\n");
    }

    printf("n elem: %lld\n", ht->header.n_ele);
    printf("\n");
    printf("Actual elem count: %lld \n",real_num);
}


//Hashtable functions----------------------------------------------------------------------------------------------------------
//hash simples 
// n -> nr base 2
// x % n == x & n-1 --- só funciona porque os nr de buckets é uma potência de base 2
size_t hash(size_t key, int64_t size) {
    return  key & (size-1);
}

hashtable* create_table(int64_t s) {
    hashtable* h = (hashtable*)malloc(sizeof(hash_header) + sizeof(BUCKET)*s);
    h->header.n_buckets = s;
    LOCK_INIT(&h->header.lock, NULL);
    h->header.n_ele = 0;
    h->header.mode = 0;
    for(int64_t i = 0; i<s; i++) {
        (&h->bucket)[i].array = (size_t*)malloc(sizeof(size_t)*ARRAY_INIT_SIZE);
        (&h->bucket)[i].n=0;
        (&h->bucket)[i].size=ARRAY_INIT_SIZE;
        //(&h->bucket)[i].first = (void*)h;
        LOCK_INIT(&(&h->bucket)[i].lock_b, NULL);
    }
    return h;
}

//data structure to keep hashtable head pointer to use the benchmark
access* create_acess(int64_t s, int64_t n_threads) {
    access* entry = (access*) malloc(sizeof(access));
    entry->ht = create_table(s);
    //entry->header.insert_count = (counter*)malloc(sizeof(counter)*MAXTHREADS);

    LOCK_INIT( &entry->header.lock, NULL);
    LOCK_INIT( &entry->lock,NULL);
    entry->header.thread_id = 0;

    for(int64_t i=0; i<MAXTHREADS; i++) {
        //printf("header counter %ld : %p = 0 \n",i, &entry->header.insert_count[i]);
        entry->header.insert_count[i].count = 0;
        entry->header.insert_count[i].ops = 0;
        entry->header.insert_count[i].header = s;
        entry->header.insert_count[i].ht_header_lock_count = 0;
    }

    return entry;
}

//pôr aqui um cas ?
int64_t get_thread_id(access* entry_point) {
    WRITE_LOCK(&entry_point->header.lock);
    
    //printf("lock \n");
	int thread_id = entry_point->header.thread_id;
    entry_point->header.thread_id++;
    //printf("Thread ID: %d \n",thread_id);
    
    UNLOCK(&entry_point->header.lock);

    return thread_id;
}
