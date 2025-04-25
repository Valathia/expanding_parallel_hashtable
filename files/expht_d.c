#include "delayed_updates_nodes.h"

/*---------------------------------------------------------------------------------------------------------------
    Expandable Hashtable with Nodes at Bucket Level
    Closed Addressing
    Delayed Header Updates
    Full Co-Op Expansion:   If a table is already expanding, Help the thread doing the expansion,
                            Go through the whole table using TryLocks and expand the buckets wher a Lock is won
                            first thread to sweep the table marks it as "finished", so other threads don't waste
                            time trying again.
                            Since the main expanding thread is sweeping the table one bucket at a time with regular locks
                            this ensures that no bucket will be left behind unchecked. 
/---------------------------------------------------------------------------------------------------------------*/

void coop_expand(hashtable* oldB, hashtable* newB, node* node_mask, int64_t i, access* entry, int64_t id_ptr) {
    
    int64_t oldK = oldB->header.n_buckets;

    //clear counter if the thread doesn't know about the new header already
    if(entry->header.insert_count[id_ptr].header <  newB->header.n_buckets) {
        entry->header.insert_count[id_ptr].count = 0;
        entry->header.insert_count[id_ptr].ops = 0;
        entry->header.insert_count[id_ptr].header = newB->header.n_buckets;
    }

    while (i < oldK) {

        if(!is_bucket_array((&oldB->bucket)[i].first)) {

            if(!TRY_LOCK(&((&oldB->bucket)[i].lock_b))) {

                //recheck after gaining the lock
                if ((&oldB->bucket)[i].first != (void*)oldB && !is_bucket_array((&oldB->bucket)[i].first)) { //&& (&oldB->bucket)[i].first != node_mask
                    
                    adjustNodes((&oldB->bucket)[i].first,newB,oldB,entry,id_ptr);
                    (&oldB->bucket)[i].first = node_mask;
                }  

                UNLOCK(&((&oldB->bucket)[i].lock_b));
            }
        }
        i++;
    }

    return;
}

int64_t insert(hashtable* b, access* entry, node* n, int64_t id_ptr) {
    
    size_t value = n->value;

    b = find_bucket(b,value);
    size_t h = Hash(value,b->header.n_buckets);
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
        count = header_update(entry,b,count,id_ptr);
    }

    return count;
}

// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
int64_t main_hash(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;

    int64_t exp_thread = b->header.mode;

    if(exp_thread > 0) {
        if(is_bucket_array((&b->bucket)[0].first)) {
            hashtable* newb = Unmask((&b->bucket)[0].first);
            coop_expand(b,newb,(&b->bucket)[0].first,entry->header.insert_count[exp_thread].expansion_at_bucket,entry,id_ptr);
            
            //the first one to finish helping marks as expanded
            if( b->header.mode>0) {
                WRITE_LOCK(&b->header.lock);
                b->header.mode = -1;
                UNLOCK(&b->header.lock);
            }

            //dont check expansion imediately after helping expand
            node* n = inst_node(value,b);
            insert(newb,entry,n,id_ptr);
            return 1;
        }
    }

    node* n = inst_node(value,b);
    int64_t chain_size = insert(b,entry,n,id_ptr);


    //pre-check to not acquire lock unecessarilly
    if((chain_size > TRESH ) && !(b->header.mode) && (b->header.n_ele > b->header.n_buckets)){

        WRITE_LOCK(&b->header.lock);
        if(!(b->header.mode) && (b->header.n_ele > b->header.n_buckets)) { 
                //signal expansion has started
                b->header.mode = id_ptr;
                UNLOCK(&b->header.lock);
                entry->header.insert_count[id_ptr].ht_header_lock_count++;

                hashtable* oldb = b;
                b = HashExpansion(b,entry,id_ptr);
                                
                //signal expansion is finished
                WRITE_LOCK(&oldb->header.lock);
                oldb->header.mode = -1;
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
