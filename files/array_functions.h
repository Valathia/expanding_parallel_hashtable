#define ARRAY 1
#include "common_functions.h"

//Prints
int64_t imprimir_array(hashtable* b, int64_t index, FILE* f){

    for(int64_t i=0; i<(&b->bucket)[index].n; i++) {
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

hashtable* create_table(int64_t s) {
    hashtable* h = (hashtable*)malloc(sizeof(hash_header) + sizeof(BUCKET)*s);
    h->header.n_buckets = s;
    LOCK_INIT(&h->header.lock, NULL);
    h->header.n_ele = 0;
    h->header.mode = 0;
    for(int64_t i = 0; i<s; i++) {
        (&h->bucket)[i].array = (void*)h;
        (&h->bucket)[i].n=0;
        (&h->bucket)[i].size=0;
        LOCK_INIT(&(&h->bucket)[i].lock_b, NULL);
    }
    return h;
}
//------------ Common Operations Across all Array Structs
size_t* expand_insert(hashtable* b, size_t value, size_t h, int64_t id_ptr) {
    size_t* old_bucket = (&b->bucket)[h].array;
    int64_t newsize = (&b->bucket)[h].size*2;
    //imprimir_array(b,h);
    size_t* new_bucket = (size_t*)malloc(sizeof(size_t)*newsize);

    int64_t i=0;
    while(i<(&b->bucket)[h].n) {
        new_bucket[i] = old_bucket[i];
        i++;
    }

    new_bucket[i] = value;


    (&b->bucket)[h].n = i+1;
    (&b->bucket)[h].size = newsize;
    free(old_bucket);
    return new_bucket;
}

void adjustNodes(BUCKET bucket, hashtable* b, support* entry, int64_t id_ptr) {


    if ( bucket.n != 0 && !is_bucket_array(bucket.array)) { 
        for(int64_t i=0; i<bucket.n ; i++) {
            insert(b,entry,bucket.array[i],id_ptr);
        }
    }
    return;
}

// função de hash expansion - nas versões com CO-OP precisa de uma verificação extra
// como não faz diferença para a normal, fica essa versão para todas
hashtable* HashExpansion(hashtable* b,  support* entry, int64_t id_ptr) {
    hashtable* oldB = b;
    int64_t oldK = b->header.n_buckets;
    int64_t newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int64_t i=0;
    size_t* node_mask = Mask(newB);

    //Reset to new table before starting (using regular insert)
    entry->header.insert_count[id_ptr].count = 0;
    entry->header.insert_count[id_ptr].ops = 0;
    entry->header.insert_count[id_ptr].header = newK;
    entry->header.insert_count[id_ptr].expansion_at_bucket = 0;

    while (i < oldK) {

        //only lock a bucket and work with it if hasn't already been expanded
        if (!is_bucket_array((&oldB->bucket)[i].array)) {

            WRITE_LOCK(&((&oldB->bucket)[i].lock_b));
            
            if ((&oldB->bucket)[i].n != 0) { //&& (&oldB->bucket)[i].first != node_mask

                adjustNodes((&oldB->bucket)[i],newB,entry,id_ptr);

            }  
            
            //recheck bucket for double expansion
            if(!is_bucket_array((&oldB->bucket)[i].array)) {
                //free old array memory && point to new ht
                if((&oldB->bucket)[i].size != 0) {
                    free((&oldB->bucket)[i].array);
                }
                (&oldB->bucket)[i].array = node_mask;
            }

            UNLOCK(&((&oldB->bucket)[i].lock_b));
        }
        entry->header.insert_count[id_ptr].expansion_at_bucket++;
        i++;
    }

    return newB;
}

hashtable* find_bucket(hashtable* b,size_t value) {
    size_t h = Hash(value,b->header.n_buckets);

    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    WRITE_LOCK(bucket_lock);

    //if bucket is pointing towards a new table, keep going until we find the bucket of the current hashtable
    while(is_bucket_array((&b->bucket)[h].array)) {

        b = Unmask((&b->bucket)[h].array);
        
        h = Hash(value,b->header.n_buckets);

        UNLOCK(bucket_lock);
        bucket_lock = &(&b->bucket)[h].lock_b;
        WRITE_LOCK(bucket_lock);
    }

    return b;
}

int64_t search(support* entry, size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    //get current hashtable where correct bucket is
    b = find_bucket(b,value);
    size_t h = Hash(value,b->header.n_buckets);

    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    
    int64_t i = 0;
    while(value!=(&b->bucket)[h].array[i] && i < (&b->bucket)[h].n) {
        i++;
    }


    size_t stop_value = (&b->bucket)[h].array[i];
    int64_t size =  (&b->bucket)[h].n;
    UNLOCK(bucket_lock);  

    if(entry->header.insert_count[id_ptr].ops >= UPDATE) {

        if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
            header_update(entry,b, 0, id_ptr);
        }
    }

    return stop_value==value && i<size;

}

int64_t delete(support* entry, size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    b = find_bucket(b,value);

    size_t h = Hash(value,b->header.n_buckets);

    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;

    int64_t i = 0;
    while(value!=(&b->bucket)[h].array[i] && i < (&b->bucket)[h].n) {
        i++;
    }

    if( (&b->bucket)[h].array[i]==value && i< (&b->bucket)[h].n) {
        //take the last element and put it at the index of the deleted element
        (&b->bucket)[h].array[i] = (&b->bucket)[h].array[(&b->bucket)[h].n-1];

        (&b->bucket)[h].n--;
        UNLOCK(bucket_lock);

        if(entry->header.insert_count[id_ptr].header == b->header.n_buckets) {
            entry->header.insert_count[id_ptr].count--;
            entry->header.insert_count[id_ptr].ops++;
        }
        else if(entry->header.insert_count[id_ptr].header < b->header.n_buckets) {
            entry->header.insert_count[id_ptr].count = -1;
            entry->header.insert_count[id_ptr].ops = 1;
            entry->header.insert_count[id_ptr].header = b->header.n_buckets;
        }
        
        //only update after certain nr of operations (UPDATE) have been done 
        if(entry->header.insert_count[id_ptr].ops >= UPDATE) {

            if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
                header_update(entry,b, 0, id_ptr);
            }
        }

        return 1;
    }


    //couldn't find value to delete    
    UNLOCK(bucket_lock);
    return 0;
}