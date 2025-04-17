#include "config.h"

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
        size_t* vec =(size_t*)malloc(sizeof(size_t)*n);
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
    hashtable* h = (hashtable*)malloc(sizeof(hash_header) + sizeof(BUCKET)*s);
    h->header.n_buckets = s;
    LOCK_INIT(&h->header.lock, NULL);
    h->header.n_ele = 0;
    h->header.mode = 0;
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
    //entry->header.insert_count = (counter*)malloc(sizeof(counter)*MAXTHREADS);
    LOCK_INIT( &entry->header.lock, NULL);
    LOCK_INIT( &entry->lock, NULL);
    for(int64_t i=0; i<MAXTHREADS; i++) {
        //printf("header counter %ld : %p = 0 \n",i, &entry->header.insert_count[i]);
        entry->header.insert_count[i].count = 0;
        entry->header.insert_count[i].ops = 0;
        entry->header.insert_count[i].header = s;
        entry->header.insert_count[i].ht_header_lock_count = 0;
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