#include "node_functions.h"

// Locks at buckets - no lock at header expansion
// - lock header
// - change mode to 1
// - unlock header
// - expand
// dar unlock do header, e deixar expandir, não altera o comportamento-
// new NULL = header memory pointer
// inserts from tail
// moves nodes 1 by 1 to new table 
// working for insert, search, delete at the same time

//*** VERSION CHANGES */
// -- can be called with jemalloc instead
// same as struct a with less updates ( with correct insert function) -> struct c
// same as c with force spacing in the counter array due to false sharing (id_ptr*8 per thread, max 32 threads)


//---------------------------------------------------------------------------------------------------------------

//Verifica condição de expansão actualmente:
//#nodes > #buckets && #nodesnumbucket > threshhold

//---------------------------------------------------------------------------------------------------------------

//função do paper que ajusta os nodes -> passa o marking node para o fim de cada bucket. 
void adjustNodes(node* n, hashtable* b,hashtable* old, access* entry , int64_t id_ptr) {

    if (n != (void*)old && !is_bucket_array(n)) { 
        node* chain = n;
        if ( chain->next != (void*)old && chain->next != (void*)b && !is_bucket_array(chain->next)) {
            adjustNodes(chain->next,b,old,entry,id_ptr);
        }
        insert(b,entry,n,id_ptr);
        
        return;
    }
    return;
}

// função de hash expansion
hashtable* HashExpansion(hashtable* b,  access* entry , int64_t id_ptr) {
    hashtable* oldB = b;
    int64_t oldK = b->header.n_buckets;
    int64_t newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int64_t i=0;
    node* node_mask = (void*)(uint64_t*)((uint64_t)newB | 1);

    //update thread counter for new header BEFORE starting
    entry->header.insert_count[id_ptr].header = newK;
    entry->header.insert_count[id_ptr].count = 0;
    entry->header.insert_count[id_ptr].ops = 0;

    while (i < oldK) {

        //substituir o cas por um if
        WRITE_LOCK(&((&oldB->bucket)[i].lock_b));
        
        if ((&oldB->bucket)[i].first != (void*)b) { //&& (&oldB->bucket)[i].first != node_mask
            
            adjustNodes((&oldB->bucket)[i].first,newB,oldB,entry,id_ptr);
            
        }  

        (&oldB->bucket)[i].first = node_mask;
        UNLOCK(&((&oldB->bucket)[i].lock_b));

        i++;
    }

    if ((entry->header.insert_count[id_ptr].count > 0) && !(newB->header.mode)) { 
        // dump last ops in header
        entry->header.insert_count[id_ptr].ht_header_lock_count++;

        WRITE_LOCK(&newB->header.lock);
            if(!(newB->header.mode)) {
                newB->header.n_ele += entry->header.insert_count[id_ptr].count;
                //*node_count;
            }
        UNLOCK(&newB->header.lock);
    }
    //reset thread counter
    entry->header.insert_count[id_ptr].count = 0;
    entry->header.insert_count[id_ptr].ops = 0;

    return newB;
}


hashtable* find_bucket_read(hashtable* b,size_t value) {
    size_t h = value & (b->header.n_buckets)-1;
    //hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    READ_LOCK(bucket_lock);

    //if bucket is pointing towards a new table, keep going until we find the bucket of the current hashtable
    while(is_bucket_array((&b->bucket)[h].first)) {

        b = (void *)((uint64_t)(&b->bucket)[h].first & ~(1<<0));

        h = value & (b->header.n_buckets)-1;
        //hash(value,b->header.n_buckets);

        //old = bucket_lock;
        UNLOCK(bucket_lock);
        bucket_lock = &(&b->bucket)[h].lock_b;
        READ_LOCK(bucket_lock);
        //UNLOCK(old);  
    }

    return b;
}

hashtable* find_bucket_write(hashtable* b,size_t value) {
    size_t h = value & (b->header.n_buckets)-1;
    //hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    WRITE_LOCK(bucket_lock);

    //if bucket is pointing towards a new table, keep going until we find the bucket of the current hashtable
    while(is_bucket_array((&b->bucket)[h].first)) {

        b = (void *)((uint64_t)(&b->bucket)[h].first & ~(1<<0));

        h = value & (b->header.n_buckets)-1;
        //hash(value,b->header.n_buckets);

        //old = bucket_lock;
        UNLOCK(bucket_lock);
        bucket_lock = &(&b->bucket)[h].lock_b;
        WRITE_LOCK(bucket_lock);
        //UNLOCK(old);  
    }

    return b;
}

int64_t delete(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    b = find_bucket_write(b,value);

    size_t h = value & (b->header.n_buckets)-1;
    //hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    

    node* cur = (&b->bucket)[h].first;
    node* prev = cur;
    
    while(cur != (void*)b) {
        if(cur->value == value) {
            //if node to delete is first
            if(cur == prev) {
                (&b->bucket)[h].first = cur->next;
            }
            else {
                prev->next = cur->next;
            }
            free(cur);
            UNLOCK(bucket_lock);

            entry->header.insert_count[id_ptr].count--;
            entry->header.insert_count[id_ptr].ops++;

            //only update after certain nr of operations (UPDATE) have been done 
            if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
                
                //if the header is the same as the header stored in the thread update, if not
                //an expansion already occured for the previous table and we're in a new table, ops are invalid
                if((b->header.n_buckets == entry->header.insert_count[id_ptr].header) && !(b->header.mode)) {
                    entry->header.insert_count[id_ptr].ht_header_lock_count++;

                    WRITE_LOCK(&b->header.lock);

                    //check for expansion
                    if (!(b->header.mode)) {
                        b->header.n_ele+= entry->header.insert_count[id_ptr].count;
                    }    
                    
                    UNLOCK(&b->header.lock);
                }
                else {
                    //if the header is not the same, update the header
                    entry->header.insert_count[id_ptr].header = b->header.n_buckets;
                }

                //in any case, reset to 0 (either it dumped the count, or the count is irrelevant due to ht expansion)
                entry->header.insert_count[id_ptr].count = 0;
                entry->header.insert_count[id_ptr].ops = 0;
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


int64_t search(access* entry, size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    //get current hashtable where correct bucket is
    b = find_bucket_read(b,value);
    size_t h = value & (b->header.n_buckets)-1;
    //hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    
    node* cur = (&b->bucket)[h].first;
    node* prev = cur;

    //if bucket where value should be is empty, return 0
    while(cur != (void*)b) {
        if(cur->value == value) {
            UNLOCK(bucket_lock);  
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    
    UNLOCK(bucket_lock);  
    return 0;
}

int64_t insert(hashtable* b, access* entry, node* n, int64_t id_ptr) {
    
    size_t value = n->value;
    //hashtable* b = entry->ht;

    b = find_bucket_write(b,value);
    size_t h = value & (b->header.n_buckets)-1;
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;


    node* cur = (&b->bucket)[h].first;
    node* prev = cur;

    int64_t count = 1;

    //get to end of bucket array
    while(cur != (void*)b) {
        //can't insert, value already exists
        if(cur->value == value) {
            UNLOCK(bucket_lock);  
            return 0;
        }
        prev = cur;
        cur = cur->next;
        count ++;
    }


    if(count==1) {
        n->next = (&b->bucket)[h].first;
        (&b->bucket)[h].first = n;
    }
    else {
        n->next = prev->next;
        prev->next = n;
    }

    //unlock search lock
    UNLOCK(bucket_lock);  

    entry->header.insert_count[id_ptr].count++;
    entry->header.insert_count[id_ptr].ops++;


    //only update after certain nr of operations (UPDATE) have been done 
    if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
        //printf("updating: %ld \n",entry->header.insert_count[id_ptr].count);

        //if the header is the same as the header stored in the thread update, if not
        //an expansion already occured for the previous table and we're in a new table, ops are invalid
        if((b->header.n_buckets == entry->header.insert_count[id_ptr].header) && !(b->header.mode)) {
            entry->header.insert_count[id_ptr].ht_header_lock_count++;

            //only lock if header is the same and has not begin expanding
            WRITE_LOCK(&b->header.lock);

            //recheck for expansion after gaining lock
            if (!(b->header.mode)) {
                b->header.n_ele+= entry->header.insert_count[id_ptr].count;
                //printf("updated! %ld \n",b->header.n_ele);
            }
            else {
                //expansion already occuring this way it won't check for expansion when leaving
                count = 0;
            }
            UNLOCK(&b->header.lock);
        }
        else {
            //if the header is not the same, update the header (don't need a lock to read this, this attribute never changes)
            entry->header.insert_count[id_ptr].header = b->header.n_buckets;
        }

        //in any case, reset to 0 (either it dumped the count, or the count is irrelevant due to ht expansion)
        entry->header.insert_count[id_ptr].count = 0;
        entry->header.insert_count[id_ptr].ops = 0;
    }

    return count;
}

// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
// os prints comentados nesta função são usados para fazer debuging. 
int64_t main_hash(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    node* n = inst_node(value,b);
    int64_t chain_size = insert(b,entry,n,id_ptr);

    //pre-check to not acquire lock unecessarilly
    if((chain_size > TRESH ) && !(b->header.mode) && (b->header.n_ele > b->header.n_buckets)){
        entry->header.insert_count[id_ptr].ht_header_lock_count++;

        WRITE_LOCK(&b->header.lock);
        if(!(b->header.mode) && (b->header.n_ele > b->header.n_buckets)) { 
                //signal expansion has started
                b->header.mode = 1;
                UNLOCK(&b->header.lock);
                hashtable* oldb = b;
                b = HashExpansion(b,entry,id_ptr);
                
                //signal expansion is finished
                entry->header.insert_count[id_ptr].ht_header_lock_count++;

                WRITE_LOCK(&oldb->header.lock);
                oldb->header.mode = 2;
                UNLOCK(&oldb->header.lock);

                WRITE_LOCK(&entry->lock);
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





//---------------------------------------------------------------------------------------------------------------
