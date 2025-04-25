#include "config.h"

//common functions for both array and node ht 
//access creation and thread related functions
access* create_acess(int64_t s, int64_t n_threads) {
    access* entry = (access*) malloc(sizeof(access));
    entry->ht = create_table(s);
    entry->header.thread_id = (int64_t)1;

    LOCK_INIT( &entry->header.lock, NULL);
    LOCK_INIT( &entry->lock,NULL);

    for(int64_t i=1; i<n_threads+1; i++) {
        //printf("header counter %ld : %p = 0 \n",i, &entry->header.insert_count[i]);
        entry->header.insert_count[i].count = 0;
        entry->header.insert_count[i].ops = 0;
        entry->header.insert_count[i].header = s;
        entry->header.insert_count[i].ht_header_lock_count = 0;
        entry->header.insert_count[i].expansion_at_bucket = 0;
        entry->header.insert_count[i].all_ops = 0;
    }

    return entry;
}

//pôr aqui um cas ?
int64_t get_thread_id(access* entry_point) {
    WRITE_LOCK(&entry_point->header.lock);
    
    //printf("lock \n");
	int64_t thread_id = entry_point->header.thread_id;
    entry_point->header.thread_id++;
    //printf("Thread ID: %d \n",thread_id);
    
    UNLOCK(&entry_point->header.lock);

    return thread_id;
}


//update header_counter with trylock
int64_t header_update(access* entry, hashtable* b, int64_t count, int64_t id_ptr) {

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