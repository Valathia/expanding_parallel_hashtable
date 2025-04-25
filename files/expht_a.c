#include "node_functions.h"

// Locks at buckets - no lock at header expansion
// - lock header
// - change n_elems to -1 
// - unlock header
// - expand
// literalmente por logo o h->header.n_ele = -1 
// dar unlock do header, e deixar expandir, não altera o comportamento-
// new NULL = header memory pointer
// inserts from tail
// moves nodes 1 by 1 to new table 
// working for insert, search, delete at the same time

/*---------------------------------------------------------------------------------------------------------------
    Expandable Hashtable with Nodes at Bucket Level
    Closed Addressing
    Base Case 
    Single-Thread Expansion
//---------------------------------------------------------------------------------------------------------------*/

int64_t delete(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = find_bucket( entry->ht,value);

    size_t h = Hash(value,b->header.n_buckets);
    
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

            WRITE_LOCK(&b->header.lock);
            //only write if no expansion is occuring
            if (b->header.n_ele>0) {
                b->header.n_ele--;
            }
            UNLOCK(&b->header.lock);
            entry->header.insert_count[id_ptr].ht_header_lock_count++;

            return 1;
        }

        prev = cur;
        cur = cur->next;
    }

    //couldn't find value to delete
    UNLOCK(bucket_lock);  
    return 0;
}

int64_t search(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    //get current hashtable where correct bucket is
    b = find_bucket(b,value);
    size_t h = Hash(value,b->header.n_buckets);
    
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

    WRITE_LOCK(&b->header.lock);
    //only update if an expansion hasn't begun
    if (b->header.n_ele>-1) {
        b->header.n_ele++;
    }
    UNLOCK(&b->header.lock);
    entry->header.insert_count[id_ptr].ht_header_lock_count++;

    return count;
}


// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
int64_t main_hash(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    node* n = inst_node(value,b);
    int64_t chain_size = insert(b,entry,n,id_ptr);


    if((chain_size > TRESH) && (b->header.n_ele > b->header.n_buckets)) { 
        entry->header.insert_count[id_ptr].ht_header_lock_count++;
        WRITE_LOCK(&b->header.lock);

        if((b->header.n_ele > b->header.n_buckets)) {
            //printf("which val: %d",VAL);
            b->header.n_ele = -1;
            UNLOCK(&b->header.lock);
            entry->header.insert_count[id_ptr].ht_header_lock_count++;

            hashtable* oldb = b;
            b = HashExpansion(b,entry,id_ptr);
            
            entry->header.insert_count[id_ptr].ht_header_lock_count++;

            WRITE_LOCK(&oldb->header.lock);
            //signal expansion is finished
            oldb->header.n_ele = -2;
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
