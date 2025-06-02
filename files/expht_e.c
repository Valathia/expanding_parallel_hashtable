#include "array_functions.h"
/*---------------------------------------------------------------------------------------------------------------
    Expandable Hashtable with Arrays at Bucket Level
    Closed Addressing
    Dellayed Header Updates    
    Single-Thread Expansion
//---------------------------------------------------------------------------------------------------------------*/


int64_t insert(hashtable* b, support* entry, size_t value, int64_t id_ptr) {
    
    b = find_bucket(b,value);
    size_t h = Hash(value,b->header.n_buckets);

    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;


    //if it's the first item, first allocate array
    if((&b->bucket)[h].size==0){
        (&b->bucket)[h].size = ARRAY_INIT_SIZE;
        (&b->bucket)[h].array = (size_t*)malloc(sizeof(size_t)*ARRAY_INIT_SIZE);
        (&b->bucket)[h].array[0] = value;
        (&b->bucket)[h].n = 1;
    }
    else {
        
        //expand && insert condition
        if((&b->bucket)[h].size==(&b->bucket)[h].n) {
            (&b->bucket)[h].array = expand_insert(b,value,h,id_ptr);
        }
        else {
            int64_t i = 0;

            //search to see if value already there
            while((&b->bucket)[h].array[i]!=value && i<(&b->bucket)[h].n) {
                i++;
            }
    
            if(i==(&b->bucket)[h].n) {
                (&b->bucket)[h].array[i] = value;
                (&b->bucket)[h].n++;
            }
            else {
                //already exists -- no reason to check for expansion if no op was done
                int64_t size = (&b->bucket)[h].n;
                UNLOCK(bucket_lock);  
                return 0;
            }
        }
    }

    int64_t size = (&b->bucket)[h].n;
    //unlock search lock
    UNLOCK(bucket_lock);  

    if(entry->header.insert_count[id_ptr].header == b->header.n_buckets) {
        entry->header.insert_count[id_ptr].count++;
        entry->header.insert_count[id_ptr].ops++;
    }
    else if(entry->header.insert_count[id_ptr].header < b->header.n_buckets) {
        entry->header.insert_count[id_ptr].count = 1;
        entry->header.insert_count[id_ptr].ops = 1;
        entry->header.insert_count[id_ptr].header = b->header.n_buckets;
    }


    //only update after certain nr of operations (UPDATE) have been done 
    if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
        size = header_update(entry,b, size, id_ptr);
    }

    return size;
}

// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
// os prints comentados nesta função são usados para fazer debuging. 
int64_t main_hash(support* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    int64_t chain_size = insert(b,entry,value,id_ptr);

    if((chain_size > TRESH ) && !(b->header.mode) && (b->header.n_ele > b->header.n_buckets)) {
        entry->header.insert_count[id_ptr].ht_header_lock_count++;

        WRITE_LOCK(&b->header.lock);

        if (!(b->header.mode) && (b->header.n_ele > b->header.n_buckets)) { 
                //signal expansion has started  
                b->header.mode = 1;
                UNLOCK(&b->header.lock);
                hashtable* oldb = b;
                b = HashExpansion(b,entry,id_ptr);
                
                //signal expansion is finished
                entry->header.insert_count[id_ptr].ht_header_lock_count++;
                WRITE_LOCK(&oldb->header.lock);
                oldb->header.mode = -1;
                UNLOCK(&oldb->header.lock);

                WRITE_LOCK(&entry->lock);
                //don't need a lock here, the other 2 possible results is not expanding or expanding. if the table finishes expanding in the meantime
                //the expanding thread can update it itself once it's done
                //Need to update this one step at a time due to SEARCH function
                while(b->header.mode == -1) {
                    b = Unmask((&b->bucket)[0].array);
                }
                
                if (entry->ht->header.n_buckets < b->header.n_buckets) {
                    entry->ht = b;
                }
                UNLOCK(&entry->lock);
                return 1;
        }
        UNLOCK(&b->header.lock);
    }

    return 1;
}