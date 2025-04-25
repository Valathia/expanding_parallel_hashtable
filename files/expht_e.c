#include "array_functions.h"
/*---------------------------------------------------------------------------------------------------------------
    Expandable Hashtable with Arrays at Bucket Level
    Closed Addressing
    Dellayed Header Updates    
    Single-Thread Expansion
//---------------------------------------------------------------------------------------------------------------*/


int64_t insert(hashtable* b, access* entry, size_t value, int64_t id_ptr) {
    
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