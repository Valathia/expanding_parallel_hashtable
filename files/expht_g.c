#include "array_functions.h"

//---------------------------------------------------------------------------------------------------------------
size_t* expand_insert(hashtable* b, size_t value, size_t h, int64_t id_ptr) {
    size_t* old_bucket = (&b->bucket)[h].array;
    int64_t newsize = (&b->bucket)[h].size*2;
    //imprimir_array(b,h);
    size_t* new_bucket = (size_t*)malloc(sizeof(size_t)*newsize);

    int i=0;
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

//don't have to check if same value has already been inserted
//already positioned in the correct hashtable
//values are already being inserted in order
//need to check for array expansion though
void adjustNodes(BUCKET bucket, hashtable* b, access* entry, int64_t id_ptr) {


    if ( bucket.n != 0 && !is_bucket_array(bucket.array)) { 
        for(int i=0; i<bucket.n ; i++) {
            insert(b,entry,bucket.array[i],id_ptr);
        }
    }
    return;
}

// função de hash expansion
hashtable* HashExpansion(hashtable* b,  access* entry, int64_t id_ptr) {
    hashtable* oldB = b;
    int64_t oldK = b->header.n_buckets;
    int64_t newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int64_t i=0;
    size_t* node_mask = (void*)(uint64_t*)((uint64_t)newB | 1);

    //Reset to new table before starting (using regular insert)
    entry->header.insert_count[id_ptr].count = 0;
    entry->header.insert_count[id_ptr].ops = 0;
    entry->header.insert_count[id_ptr].header = newK;

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
                free((&oldB->bucket)[i].array);
                (&oldB->bucket)[i].array = node_mask;
            }

            UNLOCK(&((&oldB->bucket)[i].lock_b));
        }
        i++;
    }

    return newB;
}



hashtable* find_bucket_read(hashtable* b,size_t value) {
    size_t h = value & (b->header.n_buckets)-1;
    //hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    READ_LOCK(bucket_lock);

    //if bucket is pointing towards a new table, keep going until we find the bucket of the current hashtable
    while(is_bucket_array((&b->bucket)[h].array)) {

        b = (void *)((uint64_t)(&b->bucket)[h].array & ~(1<<0));

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
    while(is_bucket_array((&b->bucket)[h].array)) {

        b = (void *)((uint64_t)(&b->bucket)[h].array & ~(1<<0));

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

int64_t search(access* entry, size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    //get current hashtable where correct bucket is
    b = find_bucket_read(b,value);
    size_t h = value & (b->header.n_buckets)-1;
    //hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    
    int i = 0;
    while(value!=(&b->bucket)[h].array[i] && i < (&b->bucket)[h].n) {
        i++;
    }


    size_t stop_value = (&b->bucket)[h].array[i];
    int64_t size =  (&b->bucket)[h].n;
    UNLOCK(bucket_lock);  

    if(entry->header.insert_count[id_ptr].ops >= UPDATE) {

        if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
            //search doesn't use counts return value. BUT IT DOES USE SIZE
            int32_t count = 0;
            header_update(entry,b, count, id_ptr);
        }   
    }


    return stop_value==value && i<size;

}

int64_t insert(hashtable* b, access* entry, size_t value, int64_t id_ptr) {
    
    b = find_bucket_write(b,value);
    size_t h = value & (b->header.n_buckets)-1;
    // hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;

    //if we're already in the hastable with a bucket lock and that table is expanding, that means this bucket can be expanded into the next table
    if(b->header.mode) {
        //you can however, be in the first bucket, in that case you can't know which one is the new table. 
        //the first bucket also needs to already be pointing to the new table
        if(h!=0 && is_bucket_array((&b->bucket)[0].array)) {

            //get new hashtable
            hashtable* newb = (void *)((uint64_t)(&b->bucket)[0].array & ~(1<<0));

            //clear counter if the thread doesn't know about the new header already
            if(entry->header.insert_count[id_ptr].header <  newb->header.n_buckets) {
                entry->header.insert_count[id_ptr].count = 0;
                entry->header.insert_count[id_ptr].ops = 0;
                entry->header.insert_count[id_ptr].header = newb->header.n_buckets;
            }

            
            //move nodes
            adjustNodes((&b->bucket)[h],newb,entry,id_ptr);
            //insert new node into newtable
            int64_t count = insert(newb,entry,value,id_ptr);
            
            //free old bucket & set node mask
            free((&b->bucket)[h].array);
            (&b->bucket)[h].array = (&b->bucket)[0].array;
            b = newb;

            UNLOCK(bucket_lock);
            return count;
        }
    }

    //expand && insert condition
    if((&b->bucket)[h].size==(&b->bucket)[h].n) {
        (&b->bucket)[h].array = expand_insert(b,value,h,id_ptr);
    }
    else {
        
        int i = 0;

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
            int32_t size = (&b->bucket)[h].n;
            UNLOCK(bucket_lock);  
            return 0;
        }
    }

    int32_t size = (&b->bucket)[h].n;
    //unlock search lock
    UNLOCK(bucket_lock);  

    entry->header.insert_count[id_ptr].count++;
    entry->header.insert_count[id_ptr].ops++;

    //only update after certain nr of operations (UPDATE) have been done 
    if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
        size = header_update(entry,b, size, id_ptr);
    }

    return size;
}

int64_t delete(access* entry, size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    b = find_bucket_write(b,value);

    size_t h = value & (b->header.n_buckets)-1;
    //hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;

    int i = 0;
    while(value!=(&b->bucket)[h].array[i] && i < (&b->bucket)[h].n) {
        i++;
    }

    if( (&b->bucket)[h].array[i]==value && i< (&b->bucket)[h].n) {
        //take the last element and put it at the index of the deleted element
        (&b->bucket)[h].array[i] = (&b->bucket)[h].array[(&b->bucket)[h].n-1];

        (&b->bucket)[h].n--;
        UNLOCK(bucket_lock);

        entry->header.insert_count[id_ptr].count--;
        entry->header.insert_count[id_ptr].ops++;

        //only update after certain nr of operations (UPDATE) have been done 
        if(entry->header.insert_count[id_ptr].ops >= UPDATE) {

            if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
                int32_t size = 0;
                header_update(entry,b, size, id_ptr);
            }
        }

        return 1;
    }


    //couldn't find value to delete    
    UNLOCK(bucket_lock);
    return 0;
}

// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
// os prints comentados nesta função são usados para fazer debuging. 
int64_t main_hash(access* entry,size_t value, int64_t id_ptr) {
    hashtable* b = entry->ht;
    int64_t chain_size = insert(b,entry,value,id_ptr);
    //quando uma thread insere uma key, se quando entrou o n_elem != -1 e quando sai é ==-1,
    //isto quer dizer que a table expandiu após a inserção, no mommento de expansão
    //à uma recontagem dos elementos à medida que são ajustados na nova tabela
    //logo, a thread não pode aumentar o nr de elementos visto que já foi contado pela expansão
    // se quando entrou o n_ele == -1, o mecanismo que lida com isso está na função de inserção 
    // se quando voltou o valor é !=-1, deve incrementar e verificar se precisa de ser expandida. 


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