#define ARRAY 1
#include "config.h"

//função que verifica se o bucket está a apontar para uma hastable nova 
size_t is_bucket_array(size_t* first) {
    return (uint64_t)first&1;
}

//imprime os nodes 
    //imprime os nodes 
    int imprimir_array(hashtable* b, int64_t index, FILE* f){

        for(int i=0; i<(&b->bucket)[index].n; i++) {
            if(i < (&b->bucket)[index].n-1) {
                fprintf(f,"%zu \n ",(&b->bucket)[index].array[i]);
            }
            else {
                fprintf(f,"%zu \n",(&b->bucket)[index].array[i]);
            }
        }
        return (&b->bucket)[index].n;
    }

    void imprimir_hash(hashtable* ht,FILE* f) {
        int64_t real_num = 0;
        fprintf(f,"-------- HASHTABLE %lld --------\n",ht->header.n_buckets);
        for(int64_t i=0;i<ht->header.n_buckets;i++) {
            
            fprintf(f,"index : %lld | array size: %lld | # elem: %lld \n",i,(&ht->bucket)[i].size,(&ht->bucket)[i].n);
            fprintf(f,"--------------------------------------------------------\n");

            real_num += imprimir_array(ht,i,f);
            
            fprintf(f,"--------------------------------------------------------\n");
        }

        fprintf(f,"n elem: %lld\n", ht->header.n_ele);
        fprintf(f,"\n");
        fprintf(f,"Actual elem count: %lld \n",real_num);
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


//update header_counter with trylock
int32_t header_update(access* entry, hashtable* b, int32_t count, int64_t id_ptr) {

    //if the header is the same as the header stored in the thread update, if not
    //an expansion already occured for the previous table and we're in a new table, ops are invalid
    if((b->header.n_buckets == entry->header.insert_count[id_ptr].header) && !(b->header.mode)) {

        //only lock if header is the same and has not begin expanding
        if (!TRY_LOCK(&b->header.lock)) {
            entry->header.insert_count[id_ptr].ht_header_lock_count++;

            //recheck for expansion after gaining lock
            if (!(b->header.mode)) {
                b->header.n_ele+= entry->header.insert_count[id_ptr].count;
                //printf("updated! %ld \n",b->header.n_ele);
            }
            else {
                //expansion already occuring this way it won't check for expansion when leaving
                count = 0;
            }
    
            entry->header.insert_count[id_ptr].count = 0;
            entry->header.insert_count[id_ptr].ops = 0;
    
            UNLOCK(&b->header.lock);
        }
    }
    else {
        //if the header is not the same, update the header (don't need a lock to read this, this attribute never changes)
        entry->header.insert_count[id_ptr].header = b->header.n_buckets;
        entry->header.insert_count[id_ptr].count = 0;
        entry->header.insert_count[id_ptr].ops = 0;
    }

    return count;
}