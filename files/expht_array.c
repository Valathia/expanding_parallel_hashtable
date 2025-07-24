#include "array_functions.h"
/*---------------------------------------------------------------------------------------------------------------
    Expandable Hashtable with Arrays at Bucket Level
    Closed Addressing
    Dellayed Header Updates    
    Co-Op Expansion:   If a table is already expanding, when inserting into a bucket,
                            Transfer the whole bucket to the new hashtable before inserting. 
/---------------------------------------------------------------------------------------------------------------*/

int64_t insert(hashtable* b, support* entry, size_t value, int64_t id_ptr) {
    
    b = find_bucket(b,value);
    size_t h = Hash(value,b->header.n_buckets);
    // hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;

    //if we're already in the hastable with a bucket lock and that table is expanding, that means this bucket can be expanded into the next table
    if(b->header.mode) {
        //you can however, be in the first bucket, in that case you can't know which one is the new table. 
        //the first bucket also needs to already be pointing to the new table
        if(h!=0 && IsHashRef((&b->bucket)[0].array)) {

            //get new hashtable
            hashtable* newb = Unmask((&b->bucket)[0].array);
    
            //clear counter if the thread doesn't know about the new header already
            if(entry->header.insert_count[id_ptr].header <  newb->header.n_buckets) {
                entry->header.insert_count[id_ptr].count = 0;
                entry->header.insert_count[id_ptr].ops = 0;
                entry->header.insert_count[id_ptr].header = newb->header.n_buckets;
            }

            if((&b->bucket)[h].size>0) {
                //move nodes
                adjustNodes((&b->bucket)[h],newb,entry,id_ptr);
                //free old bucket 
                free((&b->bucket)[h].array);
            }

            //set node mask
            (&b->bucket)[h].array = (&b->bucket)[0].array;
            UNLOCK(bucket_lock);

            b = newb;
            //insert new node into newtable
            return insert(b,entry,value,id_ptr);
        }
    }

    //if it's the first item, first allocate array
    if((&b->bucket)[h].size==0){
        (&b->bucket)[h].size = ARRAY_INIT_SIZE;
        (&b->bucket)[h].array = (size_t*)malloc(sizeof(size_t)*ARRAY_INIT_SIZE*2);
        (&b->bucket)[h].array[0] = value;
        (&b->bucket)[h].n = 1;
    }
    else {
        //expand && insert condition
        if((&b->bucket)[h].size==(&b->bucket)[h].n) {
            int oldn = (&b->bucket)[h].n;
            (&b->bucket)[h].array = expand_insert(b,value,h,id_ptr);
            //if # of elems in the array doesn't change, the new key was already in the array
            if(oldn == (&b->bucket)[h].n) {
                UNLOCK(bucket_lock);  
                return 0;
            }
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

    
    if((Cond1(chain_size)) && !(b->header.mode) && Cond2(b->header.n_ele,b->header.n_buckets)) {

        if(!TRY_LOCK(&b->header.lock)) {
            //recheck 
            if (!(b->header.mode) && Cond2(b->header.n_ele,b->header.n_buckets)) { 
                //signal expansion has started  
                b->header.mode = 1;
                UNLOCK(&b->header.lock);

                hashtable* oldb = b;
                b = HashExpansion(b,entry,id_ptr);
                
                //signal expansion is finished
                WRITE_LOCK(&oldb->header.lock);
                oldb->header.mode = -1;
                UNLOCK(&oldb->header.lock);

                WRITE_LOCK(&entry->lock);
                if (entry->ht->header.n_buckets == oldb->header.n_buckets) {
                    //Need to update this one step at a time due to SEARCH function
                    while(b->header.mode == -1) {
                        b = Unmask((&b->bucket)[0].array);
                    }
                    entry->ht = b;
                }
                UNLOCK(&entry->lock);
                return 1;
            }
            UNLOCK(&b->header.lock);
        }
    }

    return 1;
}