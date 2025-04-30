#include "common_functions.h"

//Prints
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
void imprimir_node( node *first, hashtable* b, FILE* f){
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
            fprintf(f,"%ld \n",vec[i]);
        }
    }
}

//imprime a hashtable
void imprimir_hash(hashtable* ht, FILE* f) {

    fprintf(f,"-------- HASHTABLE %lld --------\n",ht->header.n_buckets);
    for(int64_t i=0;i<ht->header.n_buckets;i++) {
        
        fprintf(f,"index : %lld\n",i);
        fprintf(f,"---------------------------\n");

        imprimir_node((&ht->bucket)[i].first,ht,f);
        
        fprintf(f,"---------------------------\n");
    }

    fprintf(f,"n elem: %lld\n", ht->header.n_ele);
    fprintf(f,"\n");

}



//NODE functions--------------------------------------------------------------------------------------------------------------------

//instancia um node*
node *inst_node(size_t k, hashtable* b){    
    node *n = (node *) malloc(sizeof(node));
    n->value = k;
    n->next=(void*)b;
    return n;
};


//função que verifica se o bucket está a apontar para uma hastable nova 
// size_t is_bucket_array(node* first) {
//     return (uint64_t)first&1;
// }


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

//Hashtable functions----------------------------------------------------------------------------------------------------------

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

//Funções comuns às HT com Nodes ----------------------------------------------------------------------------------------------------------

void adjustNodes(node* n, hashtable* b,hashtable* old, support* entry, int64_t id_ptr) {

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
// o 1º modelo faz algumas verificações desnecessárias para expansões concorrentes 
// mas não altera o comportamento para os modelos sem co-op
hashtable* HashExpansion(hashtable* b,  support* entry , int64_t id_ptr) {
    hashtable* oldB = b;
    int64_t oldK = b->header.n_buckets;
    int64_t newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int64_t i=0;
    node* node_mask = Mask(newB);


    //update thread counter for new header BEFORE starting
    entry->header.insert_count[id_ptr].header = newK;
    entry->header.insert_count[id_ptr].count = 0;
    entry->header.insert_count[id_ptr].ops = 0;
    entry->header.insert_count[id_ptr].expansion_at_bucket = 0;
    
    while (i < oldK) {

        if(!is_bucket_array((&oldB->bucket)[i].first)) {

            WRITE_LOCK(&((&oldB->bucket)[i].lock_b));
            
            if ((&oldB->bucket)[i].first != (void*)b) { //&& (&oldB->bucket)[i].first != node_mask
                
                adjustNodes((&oldB->bucket)[i].first,newB,oldB,entry,id_ptr);
                
            }  

            //resheck bucket for double expansion, just to be safe
            if(!is_bucket_array((&oldB->bucket)[i].first)) {
                (&oldB->bucket)[i].first = node_mask;
            }

            UNLOCK(&((&oldB->bucket)[i].lock_b));
        }
        entry->header.insert_count[id_ptr].expansion_at_bucket++;
        i++;
    }

    return newB;
}


hashtable* find_bucket(hashtable* b,size_t value) {
    size_t h = Hash(value,b->header.n_buckets);

    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    WRITE_LOCK(bucket_lock);

    //if bucket is pointing towards a new table, keep going until we find the bucket of the current hashtable
    while(is_bucket_array((&b->bucket)[h].first)) {

        b = Unmask((&b->bucket)[h].first);

        h = Hash(value,b->header.n_buckets);

        //old = bucket_lock;
        UNLOCK(bucket_lock);
        bucket_lock = &(&b->bucket)[h].lock_b;
        WRITE_LOCK(bucket_lock);
        //UNLOCK(old);  
    }

    return b;
}