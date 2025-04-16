#include "node_functions.h"

//int* elements;
//int* instructions;
//int* results;
//int* start;
//pthread_t* th;
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

//---------------------------------------------------------------------------------------------------------------

//Verifica condição de expansão actualmente:
//#nodes > #buckets && #nodesnumbucket > threshhold

//---------------------------------------------------------------------------------------------------------------


//função do paper que ajusta os nodes -> passa o marking node para o fim de cada bucket. 
void adjustNodes(node* n, hashtable* b,hashtable* old, access* entry, int64_t id_ptr) {

    if (n != (void*)old && !is_bucket_array(n)) { 
        node* chain = n;
        if ( chain->next != (void*)old && chain->next != (void*)b && !is_bucket_array(chain->next)) {
            adjustNodes(chain->next,b,old,entry,id_ptr);
        }
        
        insert(b,entry,chain,id_ptr);
        return;
    }
    return;
}

// função de hash expansion
hashtable* HashExpansion(hashtable* b, access* entry, int64_t id_ptr) {
    hashtable* oldB = b;
    int64_t oldK = b->header.n_buckets;
    int64_t newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int64_t i=0;
    node* node_mask = (void*)(uint64_t*)((uint64_t)newB | 1);
    
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

    return newB;
}

hashtable* find_bucket(hashtable* b,size_t value) {
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

    b = find_bucket(b,value);
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

            WRITE_LOCK(&b->header.lock);
            //only write if no expansion is occuring
            if (b->header.n_ele>0) {
                b->header.n_ele--;
            }
            UNLOCK(&b->header.lock);
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


    b = find_bucket(b,value);
    size_t h = value & (b->header.n_buckets)-1;
    // hash(value,b->header.n_buckets);
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

    return count;
}


// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
// os prints comentados nesta função são usados para fazer debuging. 
int64_t main_hash(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    node* n = inst_node(value,b);
    int64_t chain_size = insert(b,entry,n,id_ptr);


    if((chain_size > TRESH) && (b->header.n_ele > b->header.n_buckets)) { 
        WRITE_LOCK(&b->header.lock);

        if((b->header.n_ele > b->header.n_buckets)) {
            //printf("which val: %d",VAL);
            b->header.n_ele = -1;
            UNLOCK(&b->header.lock);

            hashtable* oldb = b;
            b = HashExpansion(b,entry,id_ptr);
            
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
