#include "expht_d.h"

//função que verifica se o bucket está a apontar para uma hastable nova 
size_t is_bucket_array(size_t* first) {
    return (uint64_t)first&1;
}

//imprime os nodes 
int imprimir_array(hashtable* b, int64_t index){

    for(int i=0; i<(&b->bucket)[index].n; i++) {
        if(i < (&b->bucket)[index].n-1) {
            printf("%lld , ",(&b->bucket)[index].array[i]);
        }
        else {
            printf("%lld \n",(&b->bucket)[index].array[i]);
        }
    }
    return (&b->bucket)[index].n;
}

void imprimir_hash(hashtable* ht) {
    int64_t real_num = 0;
    printf("-------- HASHTABLE %lld --------\n",ht->header.n_buckets);
    for(int64_t i=0;i<ht->header.n_buckets;i++) {
        
        printf("index : %lld\n",i);
        printf("---------------------------\n");

        real_num += imprimir_array(ht,i);
        
        printf("---------------------------\n");
    }

    printf("n elem: %lld\n", ht->header.n_ele);
    printf("\n");
    printf("Actual elem count: %lld \n",real_num);
}


//Hashtable functions----------------------------------------------------------------------------------------------------------
//hash simples 
// n -> nr base 2
// x % n == x & n-1 --- só funciona porque os nr de buckets é uma potência de base 2
size_t hash(size_t key, int64_t size) {
    return  key & (size-1);
}

hashtable* create_table(int64_t s) {
    hashtable* h = (hashtable*)malloc(sizeof(hash_header) + sizeof(array_bucket)*s);
    h->header.n_buckets = s;
    LOCK_INIT(&h->header.lock, NULL);
    h->header.n_ele = 0;
    h->header.mode = 0;
    for(int64_t i = 0; i<s; i++) {
        (&h->bucket)[i].array = (size_t*)malloc(sizeof(size_t)*ARRAY_INIT_SIZE);
        (&h->bucket)[i].n=0;
        (&h->bucket)[i].size=ARRAY_INIT_SIZE;
        //(&h->bucket)[i].first = (void*)h;
        LOCK_INIT(&(&h->bucket)[i].lock_b, NULL);
    }
    return h;
}

//data structure to keep hashtable head pointer to use the benchmark
access* create_acess(int64_t s, int64_t n_threads) {
    access* entry = (access*) malloc(sizeof(access));
    entry->ht = create_table(s);

    entry->header.thread_id = 0;
    LOCK_INIT( &entry->header.lock, NULL);
    for(int64_t i=0; i<MAXTHREADS; i++) {
        //printf("header counter %ld : %p = 0 \n",i, &entry->header.insert_count[i]);
        entry->header.insert_count[i].count = 0;
        entry->header.insert_count[i].ops = 0;
        entry->header.insert_count[i].header = s;
    }

    return entry;
}

//pôr aqui um cas ?
int64_t get_thread_id(access* entry_point) {
    WRITE_LOCK(&entry_point->header.lock);
    
    //printf("lock \n");
	int thread_id = entry_point->header.thread_id;
    entry_point->header.thread_id++;
    //printf("Thread ID: %d \n",thread_id);
    
    UNLOCK(&entry_point->header.lock);

    return thread_id;
}

//---------------------------------------------------------------------------------------------------------------

size_t* expand_insert(hashtable* b, size_t value, size_t h, int64_t id_ptr) {
    size_t* old_bucket = (&b->bucket)[h].array;
    int64_t newsize = (&b->bucket)[h].size*2;
    //imprimir_array(b,h);
    size_t* new_bucket = (size_t*)malloc(sizeof(size_t)*newsize);

    int i=0;
    while(old_bucket[i] < value && i<(&b->bucket)[h].n) {
        new_bucket[i] = old_bucket[i];
        i++;
    }
    new_bucket[i] = value;
    int j=i;
    i++;

    while(j<(&b->bucket)[h].n) {
        new_bucket[i] = old_bucket[j];
        i++;
        j++;
    }

    (&b->bucket)[h].n = i;
    (&b->bucket)[h].size = newsize;
    free(old_bucket);
    return new_bucket;
}

//don't have to check if same value has already been inserted
//already positioned in the correct hashtable
//values are already being inserted in order
//need to check for array expansion though
void adjustNodes(array_bucket bucket, hashtable* b, int64_t id_ptr) {
    size_t h;
    int64_t cur_size;
    if ( bucket.n != 0 && !is_bucket_array(bucket.array)) { 
        for(int i=0; i<bucket.n ; i++) {
            h =  bucket.array[i] & (b->header.n_buckets)-1;
            cur_size = (&b->bucket)[h].n;
            
            WRITE_LOCK(&(&b->bucket)[h].lock_b);
            if(cur_size < (&b->bucket)[h].size) {
                (&b->bucket)[h].array[cur_size] = bucket.array[i];
                (&b->bucket)[h].n++;
            }
            else {
                (&b->bucket)[h].array = expand_insert(b, bucket.array[i],h,id_ptr);
            }
            UNLOCK(&(&b->bucket)[h].lock_b);
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
    int64_t node_count = 0;

    while (i < oldK) {

        //substituir o cas por um if
        WRITE_LOCK(&((&oldB->bucket)[i].lock_b));
        
        if ((&oldB->bucket)[i].n != 0) { //&& (&oldB->bucket)[i].first != node_mask

            adjustNodes((&oldB->bucket)[i],newB,id_ptr);

        }  

        (&oldB->bucket)[i].array = node_mask;
        node_count += (&oldB->bucket)[i].n;

        UNLOCK(&((&oldB->bucket)[i].lock_b));

        i++;
    }

    // update header once with node count
    WRITE_LOCK(&newB->header.lock);
        if(!(newB->header.mode)) {
            newB->header.n_ele += node_count;
        }
    UNLOCK(&newB->header.lock);
    //reset thread counter
    entry->header.insert_count[id_ptr].count = 0;
    entry->header.insert_count[id_ptr].ops = 0;
    entry->header.insert_count[id_ptr].header = newK;
    

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
    while(value>(&b->bucket)[h].array[i] && i < (&b->bucket)[h].n) {
        i++;
    }

    size_t stop_value = (&b->bucket)[h].array[i];
    int64_t size =  (&b->bucket)[h].n;
    UNLOCK(bucket_lock);  

    return stop_value==value && i<size;

}

int64_t insert(hashtable* b, access* entry, size_t value, int64_t id_ptr) {
    
    b = find_bucket_write(b,value);
    size_t h = value & (b->header.n_buckets)-1;
    // hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;


    //expand && insert condition
    if((&b->bucket)[h].size==(&b->bucket)[h].n) {
        (&b->bucket)[h].array = expand_insert(b,value,h,id_ptr);

    }
    else {
        
        int i = 0;
        //stops when value > or =
        while((&b->bucket)[h].array[i]<value && i<(&b->bucket)[h].n) {
            i++;
        }

        //if inside current valid space
        if(i<(&b->bucket)[h].n) {
            //shimmy the values to the right and insert
            if((&b->bucket)[h].array[i] != value) {
                size_t cur_value;
                size_t prev_value = value;
                while(i<(&b->bucket)[h].n) {
                    cur_value = (&b->bucket)[h].array[i];
                    (&b->bucket)[h].array[i] = prev_value;
                    prev_value = cur_value;
                    i++;
                }
                (&b->bucket)[h].array[i] = prev_value;
                (&b->bucket)[h].n++;

            }
            //value already inserted
            else {
                int size = (&b->bucket)[h].n;
                UNLOCK(bucket_lock);  
                return size;
            }
        }
        else {
            (&b->bucket)[h].array[i] = value;
            (&b->bucket)[h].n++;
        }
    }

    int size = (&b->bucket)[h].n;
    //unlock search lock
    UNLOCK(bucket_lock);  

    entry->header.insert_count[id_ptr].count++;
    entry->header.insert_count[id_ptr].ops++;

    //only update after certain nr of operations (UPDATE) have been done 
    if(entry->header.insert_count[id_ptr].ops >= UPDATE) {
        
        WRITE_LOCK(&b->header.lock);
        //if the header is the same as the header stored in the thread update, if not
        //an expansion already occured for the previous table and we're in a new table, ops are invalid
        if(b->header.n_buckets == entry->header.insert_count[id_ptr].header) {
            //check for expansion
            if (!(b->header.mode)) {
                b->header.n_ele+= entry->header.insert_count[id_ptr].count;
            }
            else {
                //expansion already occuring this way it won't check for expansion when leaving
                size = 0;
            }
            
        }
        else {
            //if the header is not the same, update the header and update the current op (since we already have the lock)
            entry->header.insert_count[id_ptr].header = b->header.n_buckets;
            entry->ht->header.n_ele++;
        }
        UNLOCK(&b->header.lock);

        //in any case, reset to 0 (either it dumped the count, or the count is irrelevant due to ht expansion)
        entry->header.insert_count[id_ptr].count = 0;
        entry->header.insert_count[id_ptr].ops = 0;
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
    while(value>(&b->bucket)[h].array[i] && i < (&b->bucket)[h].n) {
        i++;
    }

    if( (&b->bucket)[h].array[i]==value && i< (&b->bucket)[h].n) {
        while(i < ((&b->bucket)[h].n-1)) {
            (&b->bucket)[h].array[i] = (&b->bucket)[h].array[i+1];
            i++;
        }
        (&b->bucket)[h].n--;
        UNLOCK(bucket_lock);

        entry->header.insert_count[id_ptr].count--;
        entry->header.insert_count[id_ptr].ops++;

        //only update after certain nr of operations (UPDATE) have been done 
        if(entry->header.insert_count[id_ptr].ops >= UPDATE) {

            WRITE_LOCK(&b->header.lock);
            //if the header is the same as the header stored in the thread update, if not
            //an expansion already occured for the previous table and we're in a new table, ops are invalid
            if(b->header.n_buckets == entry->header.insert_count[id_ptr].header) {
                //check for expansion
                if (!(b->header.mode)) {
                    b->header.n_ele+= entry->header.insert_count[id_ptr].count;
                }                    
            }
            else {
                //if the header is not the same, update the header and update the current op (since we already have the lock)
                entry->header.insert_count[id_ptr].header = b->header.n_buckets;
                entry->ht->header.n_ele--;
            }
            UNLOCK(&b->header.lock);

            //in any case, reset to 0 (either it dumped the count, or the count is irrelevant due to ht expansion)
            entry->header.insert_count[id_ptr].count = 0;
            entry->header.insert_count[id_ptr].ops = 0;
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


    if(chain_size > TRESH) {
        WRITE_LOCK(&b->header.lock);

        if (!(b->header.mode) && (b->header.n_ele > b->header.n_buckets)) { 
                //signal expansion has started  
                b->header.mode = 1;
                UNLOCK(&b->header.lock);
                hashtable* oldb = b;
                b = HashExpansion(b,entry,id_ptr);
                
                WRITE_LOCK(&oldb->header.lock);
                //signal expansion is finished
                b->header.mode = 2;
                UNLOCK(&oldb->header.lock);

                WRITE_LOCK(&(entry->ht->header).lock);
                oldb = entry->ht;

                if (entry->ht->header.n_buckets < b->header.n_buckets) {
                    entry->ht = b;
                }
                UNLOCK(&oldb->header.lock);
                return 1;
        }
        UNLOCK(&b->header.lock);
    }

    return 1;
}