#include "delayed_updates_nodes.h"

/*---------------------------------------------------------------------------------------------------------------
    Expandable Hashtable with Nodes at Bucket Level
    Closed Addressing
    Delayed Header Updates
    Lazy Co-Op Expansion:   If a table is already expanding, when inserting into a bucket,
                            Transfer the whole bucket to the new hashtable before inserting. 
/---------------------------------------------------------------------------------------------------------------*/



int64_t insert(hashtable* b, access* entry, node* n, int64_t id_ptr) {
    
    size_t value = n->value;

    b = find_bucket(b,value);
    size_t h = Hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;


    node* cur = (&b->bucket)[h].first;
    node* prev = cur;

    int64_t count = 1;

    //if we're already in the hastable with a bucket lock you can't be done with expansion
    if(b->header.mode) {
        //you can however, be in the first bucket, in that case you can't know which one is the new table. 
        //the first bucket also needs to already be pointing to the new table
        if(h!=0 && is_bucket_array((&b->bucket)[0].first)) {

            //get new hashtable
            hashtable* newb = Unmask((&b->bucket)[0].first);

            //clear counter if the thread doesn't know about the new header already
            if(entry->header.insert_count[id_ptr].header <  newb->header.n_buckets) {
                entry->header.insert_count[id_ptr].count = 0;
                entry->header.insert_count[id_ptr].ops = 0;
                entry->header.insert_count[id_ptr].header = newb->header.n_buckets;
            }

            
            //move nodes
            adjustNodes((&b->bucket)[h].first,newb,b,entry,id_ptr);
            //insert new node into newtable
            count = insert(newb,entry,n,id_ptr);
            
            //set node mask
            (&b->bucket)[h].first = (&b->bucket)[0].first;
            b = newb;

            UNLOCK(bucket_lock);
            return count;
        }
    }

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
    else {
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
                
                entry->header.insert_count[id_ptr].ht_header_lock_count++;
                
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
