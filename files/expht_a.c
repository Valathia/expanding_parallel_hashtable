#include "expht_a.h"

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


//NODE functions--------------------------------------------------------------------------------------------------------------------
//instancia um node*
node *inst_node(size_t k, hashtable* b){    
    node *n = (node *) malloc(sizeof(node));
    n->value = k;
    n->next=(void*)b;
    return n;
};


//função que verifica se o bucket está a apontar para uma hastable nova 
size_t is_bucket_array(node* first) {
    return (uint64_t)first&1;
}


//conta quantos nodes tem num "bucket"
int64_t bucket_size(node *first, hashtable* b) {
    if(first==(void*)b || is_bucket_array(first)) {
        return 0;
    }

    int64_t i = 0;
    node* p = NULL;
    for(p=first; p!=(void*)b && !is_bucket_array(p) ;p=p->next) {
        i++;
    }
    return i;
}

//insert sort para inserir os elementos por ordem antes de imprimir
size_t* insert_sort(size_t vec[], size_t val,int64_t size) {
    size_t aux;
    size_t auxj;
    short flag = 0;

    for(int64_t i =0; i<size; i++) {
        if(val<vec[i]) {
            aux = vec[i];
            vec[i] = val;
            flag = 1;
            size++;
            if(size-1==i+1){
                vec[++i] = aux;
            }
            else{
                for(int64_t j=i+1; j<size; j++) {
                    auxj = vec[j];
                    vec[j] = aux;
                    aux = auxj;
                }
                break;
            }
        }
    }

    if(flag == 0) {
        vec[size] = val;
        size++;
    }

    return vec;
}

//imprime os nodes 
void imprimir_node( node *first, hashtable* b){
    struct node *p = NULL;

    if(first != (void*)b) {
        int64_t n = bucket_size(first,b);
        if(n == 0) {
            return;
        }
        size_t* vec =(size_t*)calloc(sizeof(size_t),n);
        int64_t size = 1;
        vec[0] = first->value;
        for(p=first->next; p!=(void*)b && !is_bucket_array(p) ;p=p->next){
            insert_sort(vec,p->value,size);
            size++;
        }

        for(int64_t i=0; i<n; i++) {
            printf("%ld \n",vec[i]);
        }
    }
}



//Hashtable functions----------------------------------------------------------------------------------------------------------
//hash simples 
// n -> nr base 2
// x % n == x & n-1
size_t hash(size_t key, int64_t size) {

    return  key & (size-1);
}

// tentei, artificialmente, fazer com que o h->bucket aponte para ele prórpio
// depois por as posições de memória correspondetes a NULL, o problema:
// nunca posso usar bucket[i] porque o compilador vai tratar sempre como sendo um node*
// com nodes dentro, o que faz com que por exemplo, no for, o i quando incrementa 1 à memória,
// se estiver como tenho escrito, incrementa 8 que é o size de um node* mas se tentar fazer h->bucket[i]
// vai incrementar 16 que é o size de um node. 
// Ou seja, tecnicamente, tenho (node* -> NULL)*n_buckets em memória continua com a restante estrutura. 
// mas não posso usar a sintaxe normal. 
hashtable* create_table(int64_t s) {
    hashtable* h = (hashtable*)malloc(sizeof(hash_header) + sizeof(node_bucket)*s);
    h->header.n_buckets = s;
    LOCK_INIT(&h->header.lock, NULL);
    h->header.n_ele = 0;
    for(int64_t i = 0; i<s; i++) {
        (&h->bucket)[i].first = (void*)h;
        LOCK_INIT(&(&h->bucket)[i].lock_b, NULL);
    }
    return h;
}


//imprime a hashtable
void imprimir_hash(hashtable* ht) {

    printf("-------- HASHTABLE %lld --------\n",ht->header.n_buckets);
    for(int64_t i=0;i<ht->header.n_buckets;i++) {
        
        printf("index : %lld\n",i);
        printf("---------------------------\n");

        imprimir_node((&ht->bucket)[i].first,ht);
        
        printf("---------------------------\n");
    }

    printf("n elem: %lld\n", ht->header.n_ele);
    printf("\n");

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

    WRITE_LOCK(&b->header.lock);

    if((chain_size > TRESH) && (b->header.n_ele > b->header.n_buckets)) { 
            //printf("which val: %d",VAL);
            b->header.n_ele = -1;
            UNLOCK(&b->header.lock);

            hashtable* oldb = b;
            b = HashExpansion(b,entry,id_ptr);
            
            WRITE_LOCK(&oldb->header.lock);
            //signal expansion is finished
            oldb->header.n_ele = -2;
            UNLOCK(&oldb->header.lock);

            WRITE_LOCK(&entry->ht->header.lock);
            oldb = entry->ht;
            if (entry->ht->header.n_buckets < b->header.n_buckets) {
                entry->ht = b;
            }
            UNLOCK(&oldb->header.lock);
            return 1;
    }

    UNLOCK(&b->header.lock);
    return 1;
}





//---------------------------------------------------------------------------------------------------------------
