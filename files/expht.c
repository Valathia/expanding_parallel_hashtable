#include "expht.h"
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define TRESH 16

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
node *inst_node(int k, int v, hashtable* b){    
    node *n = (node *) calloc(sizeof(node),1);
    n->data.key = k;
    n->data.val = v;
    n->next=(void*)b;
    return n;
};


//função que verifica se o bucket está a apontar para uma hastable nova 
int is_bucket_array(node* first) {
    return (uint64_t)first&1;
}


//conta quantos nodes tem num "bucket"
int bucket_size(node *first, hashtable* b) {
    if(first==(void*)b || is_bucket_array(first)) {
        return 0;
    }

    int i = 0;
    node* p = NULL;
    for(p=first; p!=(void*)b && !is_bucket_array(p) ;p=p->next) {
        i++;
    }
    return i;
}

//insert sort para inserir os elementos por ordem antes de imprimir
pair* insert_sort(pair vec[], pair p,int size) {
    pair aux;
    pair auxj;
    int flag = 0;

    for(int i =0; i<size; i++) {
        if(p.key<vec[i].key) {
            aux = vec[i];
            vec[i] = p;
            flag = 1;
            size++;
            if(size-1==i+1){
                vec[++i] = aux;
            }
            else{
                for(int j=i+1; j<size; j++) {
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
        int n = bucket_size(first,b);
        if(n == 0) {
            return;
        }
        pair* vec =(pair*)calloc(sizeof(pair),n);
        int size = 1;
        vec[0] = first->data;
        for(p=first->next; p!=(void*)b && !is_bucket_array(p) ;p=p->next){
            insert_sort(vec,p->data,size);
            size++;
        }

        for(int i=0; i<n; i++) {
            printf("%d : %d \n",vec[i].key,vec[i].val);
        }
    }
}


//Hashtable functions----------------------------------------------------------------------------------------------------------
//hash simples 
int hash(int key, int size) {
    int h =  key%size;
    if(h<0)
        return (-1)*h;

    return h;
}

// tentei, artificialmente, fazer com que o h->bucket aponte para ele prórpio
// depois por as posições de memória correspondetes a NULL, o problema:
// nunca posso usar bucket[i] porque o compilador vai tratar sempre como sendo um node*
// com nodes dentro, o que faz com que por exemplo, no for, o i quando incrementa 1 à memória,
// se estiver como tenho escrito, incrementa 8 que é o size de um node* mas se tentar fazer h->bucket[i]
// vai incrementar 16 que é o size de um node. 
// Ou seja, tecnicamente, tenho (node* -> NULL)*n_buckets em memória continua com a restante estrutura. 
// mas não posso usar a sintaxe normal. 
hashtable* create_table(int s) {
    hashtable* h = (hashtable*)calloc(sizeof(hash_header) + sizeof(node_bucket)*s,1);
    h->header.n_buckets = s;
    LOCK_INIT(&h->header.lock, NULL);
    h->header.n_ele = 0;
    for(int i = 0; i<s; i++) {
        (&h->bucket)[i].first = (void*)h;
        LOCK_INIT(&(&h->bucket)[i].lock_b, NULL);
    }
    return h;
}


//imprime a hashtable
void imprimir_hash(hashtable* ht) {

    printf("-------- HASHTABLE %d --------\n",ht->header.n_buckets);
    for(int i=0;i<ht->header.n_buckets;i++) {
        
        printf("index : %d\n",i);
        printf("---------------------------\n");

        imprimir_node((&ht->bucket)[i].first,ht);
        
        printf("---------------------------\n");
    }

    printf("n elem: %d\n", ht->header.n_ele);
    printf("\n");

}


//data structure to keep hashtable head pointer to use the benchmark
access* create_acess(int s) {
    access* entry = (access*) calloc(sizeof(access),1);
    entry->ht = create_table(s);

    return entry;
}


//---------------------------------------------------------------------------------------------------------------

//Verifica condição de expansão actualmente:
//#nodes > #buckets && #nodesnumbucket > threshhold

//---------------------------------------------------------------------------------------------------------------

void bucket_transfer(node* n, hashtable* b, hashtable* old, void* id_ptr, int* r_n_elem) {
    if (n != (void*)old && !is_bucket_array(n)) { 
        node* chain = n;
        if ( chain->next != (void*)old && chain->next != (void*)b && !is_bucket_array(chain->next)) {
            bucket_transfer(chain->next,b,old,id_ptr,r_n_elem);
        }

        int h = hash(n->data.key,b->header.n_buckets);
        LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
        WRITE_LOCK(bucket_lock);
        LOCKS* old = (LOCKS*)calloc(sizeof(LOCKS),1);

        if((&b->bucket)[h].first == (void*)b) {

            n->next = (&b->bucket)[h].first;
            (&b->bucket)[h].first = n;
            WRITE_LOCK(&b->header.lock);
            if (b->header.n_ele>-1) {
                b->header.n_ele++;
            }
            UNLOCK(&b->header.lock);
            UNLOCK(bucket_lock);
            *r_n_elem = 1;
            return;
        }

        while(is_bucket_array((&b->bucket)[h].first)) {

            b = (void *)((uint64_t)(&b->bucket)[h].first & ~(1<<0));

            h = hash(n->data.key,b->header.n_buckets);

            old = bucket_lock;
            bucket_lock = &(&b->bucket)[h].lock_b;
            WRITE_LOCK(bucket_lock);
            UNLOCK(old);  
        }
        
        n->next = (&b->bucket)[h].first;
        (&b->bucket)[h].first = n;
        
        UNLOCK(bucket_lock);

        WRITE_LOCK(&b->header.lock);
        if (b->header.n_ele>-1) {
            b->header.n_ele++;
        }
        UNLOCK(&b->header.lock);
        return;
    }
    return;
}

//função do paper que ajusta os nodes -> passa o marking node para o fim de cada bucket. 
void adjustNodes(node* n, hashtable* b,hashtable* old, void* id_ptr) {

    if (n != (void*)old && !is_bucket_array(n)) { 
        node* chain = n;
        if ( chain->next != (void*)old && chain->next != (void*)b && !is_bucket_array(chain->next)) {
            adjustNodes(chain->next,b,old,id_ptr);
        }
        
        insert(b,chain,id_ptr);
        return;
    }
    return;
}



hashtable* find_bucket_read(hashtable* b,int value) {
    int h = hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    READ_LOCK(bucket_lock);

    //if bucket is pointing towards a new table, keep going until we find the bucket of the current hashtable
    while(is_bucket_array((&b->bucket)[h].first)) {

        b = (void *)((uint64_t)(&b->bucket)[h].first & ~(1<<0));

        h = hash(value,b->header.n_buckets);

        //old = bucket_lock;
        UNLOCK(bucket_lock);
        bucket_lock = &(&b->bucket)[h].lock_b;
        READ_LOCK(bucket_lock);
        //UNLOCK(old);  
    }

    return b;
}

hashtable* find_bucket_write(hashtable* b,int value) {
    int h = hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;
    WRITE_LOCK(bucket_lock);

    //if bucket is pointing towards a new table, keep going until we find the bucket of the current hashtable
    while(is_bucket_array((&b->bucket)[h].first)) {

        b = (void *)((uint64_t)(&b->bucket)[h].first & ~(1<<0));

        h = hash(value,b->header.n_buckets);

        //old = bucket_lock;
        UNLOCK(bucket_lock);
        bucket_lock = &(&b->bucket)[h].lock_b;
        WRITE_LOCK(bucket_lock);
        //UNLOCK(old);  
    }

    return b;
}

int search(hashtable* b,int value, void* id_ptr) {

    //get current hashtable where correct bucket is
    b = find_bucket_read(b,value);
    int h = hash(value,b->header.n_buckets);
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

int insert(hashtable* b, node* n, void* id_ptr) {
    
    int value = n->data.key;

    b = find_bucket_write(b,value);
    int h = hash(value,b->header.n_buckets);
    LOCKS* bucket_lock = &(&b->bucket)[h].lock_b;


    node* cur = (&b->bucket)[h].first;
    node* prev = cur;

    int count = 1;

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

    WRITE_LOCK(&b->header.lock);
    //only update if an expansion hasn't begun
    if (b->header.n_ele>-1) {
        b->header.n_ele++;
    }
    UNLOCK(&b->header.lock);

    return count;
}

int delete(hashtable* b,int value, void* id_ptr) {
    b = find_bucket_write(b,value);
    int h = hash(value,b->header.n_buckets);
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




// função de hash expansion
hashtable* HashExpansion(hashtable* b, void* id_ptr) {
    hashtable* oldB = b;
    int oldK = b->header.n_buckets;
    int newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int i=0;
    node* node_mask = (void*)(uint64_t*)((uint64_t)newB | 1);
    
    while (i < oldK) {

        //substituir o cas por um if
        WRITE_LOCK(&((&oldB->bucket)[i].lock_b));
        
        if ((&oldB->bucket)[i].first != (void*)b) { //&& (&oldB->bucket)[i].first != node_mask
            
            adjustNodes((&oldB->bucket)[i].first,newB,oldB,id_ptr);
            
        }  

        (&oldB->bucket)[i].first = node_mask;
        UNLOCK(&((&oldB->bucket)[i].lock_b));

        i++;
    }

    return newB;
}

// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
// os prints comentados nesta função são usados para fazer debuging. 
int main_hash(access* pointer,int value, void* id_ptr) {
    hashtable* b = pointer->ht;
    node* n = inst_node(value,(int)id_ptr,b);
    int chain_size = insert(b,n,id_ptr);
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
