#include "node_functions.h"
int64_t delete(support* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    b = find_bucket(b,value);

    size_t h = Hash(value,b->header.n_buckets);
    
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    
    node* cur = (&b->bucket)[h].first;
    node* prev = (void*)b;
    
    while(cur != (void*)b) {
        if(cur->value == value) {
            //if node to delete is first
            if(prev == (void*)b) {
                (&b->bucket)[h].first = cur->next;
            }
            else {
                prev->next = cur->next;
            }
            free(cur);
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
                header_update(entry,b,0,id_ptr);
            }

            return 1;
        }

        prev = cur;
        cur = cur->next;
    }

    //couldn't find value to delete
    UNLOCK(bucket_lock);  
    return 0;
}


int64_t search(support* entry, size_t value, int64_t id_ptr) {

    //get current hashtable where correct bucket is
    hashtable* b = entry->ht;
    b = find_bucket(b,value);
    
    size_t h = Hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;

    node* cur = (&b->bucket)[h].first;

    //if bucket, where value should be, is empty, return 0
    while(cur != (void*)b) {
        if(cur->value == value) {
            UNLOCK(bucket_lock);  
            
            if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
                header_update(entry,b,0,id_ptr);
            }
            return 1;
        }
        cur = cur->next;
    }
    
    UNLOCK(bucket_lock);  

    if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
        header_update(entry,b,0,id_ptr);
    }
    
    return 0;
}
