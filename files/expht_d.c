#include "expht_d.h"

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

//*** VERSION CHANGES */
// swaped calloc for malloc         -- check if this doesn't break the memory allocation method
// -- can be called with jemalloc instead
// é precis over se o update dos deletes não vai partir isto tudo

//NODE functions--------------------------------------------------------------------------------------------------------------------
//instancia um node*
node *inst_node(size_t k, size_t v, hashtable* b){    
    node *n = (node *) malloc(sizeof(node));
    n->data.key = k;
    n->data.val = v;
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
pair* insert_sort(pair vec[], pair p,int64_t size) {
    pair aux;
    pair auxj;
    short flag = 0;

    for(int64_t i =0; i<size; i++) {
        if(p.key<vec[i].key) {
            aux = vec[i];
            vec[i] = p;
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
        vec[size] = p;
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
        pair* vec =(pair*)calloc(sizeof(pair),n);
        int64_t size = 1;
        vec[0] = first->data;
        for(p=first->next; p!=(void*)b && !is_bucket_array(p) ;p=p->next){
            insert_sort(vec,p->data,size);
            size++;
        }

        for(int64_t i=0; i<n; i++) {
            printf("%ld : %ld \n",vec[i].key,vec[i].val);
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

    printf("-------- HASHTABLE %ld --------\n",ht->header.n_buckets);
    for(int64_t i=0;i<ht->header.n_buckets;i++) {
        
        printf("index : %ld\n",i);
        printf("---------------------------\n");

        imprimir_node((&ht->bucket)[i].first,ht);
        
        printf("---------------------------\n");
    }

    printf("n elem: %ld\n", ht->header.n_ele);
    printf("\n");

}


//data structure to keep hashtable head pointer to use the benchmark
access* create_acess(int64_t s, int64_t n_threads) {
    access* entry = (access*) malloc(sizeof(access));
    entry->ht = create_table(s);

    entry->header.thread_id = 0;
    LOCK_INIT( &entry->header.lock, NULL);
    for(int64_t i=0; i<64; i++) {
        //printf("header counter %ld : %p = 0 \n",i, &entry->header.insert_count[i]);
        entry->header.insert_count[i] = 0;
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

void insert_expand(hashtable* b, node* n, int64_t id_ptr) {

    size_t value = n->data.key;


    size_t h = value & (b->header.n_buckets)-1; // hash(value,b->header.n_buckets);

    node* cur = (&b->bucket)[h].first;
    node* prev = cur;

    int64_t count = 1;

    //get to end of bucket array
    while(cur != (void*)b) {
        //can't insert, value already exists
        if(cur->data.key == value) {
            return;
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

    return;
}


//função do paper que ajusta os nodes -> passa o marking node para o fim de cada bucket. 
void adjustNodes(node* n, hashtable* b,hashtable* old, int64_t id_ptr, int64_t* node_count) {

    if (n != (void*)old && !is_bucket_array(n)) { 
        node* chain = n;
        if ( chain->next != (void*)old && chain->next != (void*)b && !is_bucket_array(chain->next)) {
            adjustNodes(chain->next,b,old,id_ptr,node_count);
        }
        
        //estava a usar o insert normal -> insert não bloqueante para a expansão 
        insert_expand(b,chain,id_ptr);
        node_count++;
        return;
    }
    return;
}

// função de hash expansion
hashtable* HashExpansion(hashtable* b, int64_t id_ptr) {
    hashtable* oldB = b;
    int64_t oldK = b->header.n_buckets;
    int64_t newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int64_t i=0;
    node* node_mask = (void*)(uint64_t*)((uint64_t)newB | 1);
    int64_t* node_count = (int64_t* ) malloc(sizeof(int64_t));
    *node_count = 0;

    while (i < oldK) {

        //substituir o cas por um if
        WRITE_LOCK(&((&oldB->bucket)[i].lock_b));
        
        if ((&oldB->bucket)[i].first != (void*)b) { //&& (&oldB->bucket)[i].first != node_mask
            
            adjustNodes((&oldB->bucket)[i].first,newB,oldB,id_ptr,node_count);
            
        }  

        (&oldB->bucket)[i].first = node_mask;
        UNLOCK(&((&oldB->bucket)[i].lock_b));

        i++;
    }

    // update header once with node count
    WRITE_LOCK(&newB->header.lock);
        if(newB->header.n_ele > -1) {
            newB->header.n_ele += *node_count;
        }
    UNLOCK(&newB->header.lock);

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
        if(cur->data.key == value) {
            UNLOCK(bucket_lock);  
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    
    UNLOCK(bucket_lock);  
    return 0;
}

int64_t insert(access* entry, node* n, int64_t id_ptr) {
    
    size_t value = n->data.key;
    hashtable* b = entry->ht;

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
        if(cur->data.key == value) {
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

    entry->header.insert_count[id_ptr]++;
    //only update if thread already inserted UPDATE(1000) elements
    if(entry->header.insert_count[id_ptr] >= UPDATE || entry->header.insert_count[id_ptr] <= (-1)*UPDATE ) {

    WRITE_LOCK(&b->header.lock);
    //only update if an expansion hasn't begun
        if (b->header.n_ele>-1) {
            b->header.n_ele+= entry->header.insert_count[id_ptr];
            entry->header.insert_count[id_ptr] = 0;
        }
        UNLOCK(&b->header.lock);
    }

    return count;
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
        if(cur->data.key == value) {
            //if node to delete is first
            if(cur == prev) {
                (&b->bucket)[h].first = cur->next;
            }
            else {
                prev->next = cur->next;
            }
            UNLOCK(bucket_lock);

            entry->header.insert_count[id_ptr]--;

            if(entry->header.insert_count[id_ptr] >= UPDATE || entry->header.insert_count[id_ptr] <= (-1)*UPDATE ) {

                WRITE_LOCK(&b->header.lock);
                //only update if an expansion hasn't begun
                    if (b->header.n_ele>-1) {
                        b->header.n_ele+= entry->header.insert_count[id_ptr];
                        entry->header.insert_count[id_ptr] = 0;
                    }
                    UNLOCK(&b->header.lock);
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


// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
// os prints comentados nesta função são usados para fazer debuging. 
int64_t main_hash(access* pointer,size_t value, int64_t id_ptr) {
    hashtable* b = pointer->ht;
    node* n = inst_node(value,id_ptr,b);
    int64_t chain_size = insert(pointer,n,id_ptr);
    //quando uma thread insere uma key, se quando entrou o n_elem != -1 e quando sai é ==-1,
    //isto quer dizer que a table expandiu após a inserção, no mommento de expansão
    //à uma recontagem dos elementos à medida que são ajustados na nova tabela
    //logo, a thread não pode aumentar o nr de elementos visto que já foi contado pela expansão
    // se quando entrou o n_ele == -1, o mecanismo que lida com isso está na função de inserção 
    // se quando voltou o valor é !=-1, deve incrementar e verificar se precisa de ser expandida. 

    WRITE_LOCK(&b->header.lock);

    if((chain_size > TRESH) && (b->header.n_ele > b->header.n_buckets)) { 
            b->header.n_ele = -1;
            UNLOCK(&b->header.lock);
            hashtable* oldb = b;
            b = HashExpansion(b,id_ptr);
            
            WRITE_LOCK(&oldb->header.lock);
            //signal expansion is finished
            oldb->header.n_ele = -2;
            UNLOCK(&oldb->header.lock);

            oldb = pointer->ht;
            WRITE_LOCK(&oldb->header.lock);
            if (pointer->ht->header.n_buckets < b->header.n_buckets) {
                pointer->ht = b;
            }
            UNLOCK(&oldb->header.lock);
            return 1;
    }

    UNLOCK(&b->header.lock);
    return 1;
}





//---------------------------------------------------------------------------------------------------------------
