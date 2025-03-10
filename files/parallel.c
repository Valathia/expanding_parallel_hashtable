#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define TRESH 2


int n_els,n_threads;
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
//sshfs
//usar o benchmark do pedro no meu pc 1º  -- está no email


//ALL STRUCTS --------------------------------------------------------------------------------------------------------------------
typedef struct pair {
    int key;
    int val;
}pair;

typedef struct node {
    pair data;
    struct node* next;
}node;

typedef struct node_bucket{
    pthread_rwlock_t lock_b;
    node* first;
}node_bucket;

typedef struct hash_header {
    pthread_rwlock_t lock;
    int n_ele;
    int n_buckets;
}hash_header;

typedef struct hashtable {
    hash_header header;
    node_bucket bucket;
}hashtable;


//------- VARIAVEIS GLOBAIS

void* thread_main(void *id_ptr);
void init_hash(void *id_ptr);
void init_del(void* id_ptr);
void init_search(void* id_ptr);
hashtable* ht ;
int* seeds;
int delete_search_insert(hashtable* b,node* n, int flag, int* r_n_elem, void* id_ptr);
void trie_search_insert(hashtable* ht,node* n, void* id_ptr, int* r_n_elem);

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
    pthread_rwlock_init(&h->header.lock, NULL);
    h->header.n_ele = 0;
    for(int i = 0; i<s; i++) {
        (&h->bucket)[i].first = (void*)h;
        pthread_rwlock_init(&(&h->bucket)[i].lock_b, NULL);
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
        pthread_rwlock_t* bucket_lock = &(&b->bucket)[h].lock_b;
        pthread_rwlock_wrlock(bucket_lock);
        pthread_rwlock_t* old = (pthread_rwlock_t*)calloc(sizeof(pthread_rwlock_t),1);

        if((&b->bucket)[h].first == (void*)b) {

            n->next = (&b->bucket)[h].first;
            (&b->bucket)[h].first = n;
            pthread_rwlock_wrlock(&b->header.lock);
            if (b->header.n_ele>-1) {
                b->header.n_ele++;
            }
            pthread_rwlock_unlock(&b->header.lock);
            pthread_rwlock_unlock(bucket_lock);
            *r_n_elem = 1;
            return;
        }

        while(is_bucket_array((&b->bucket)[h].first)) {

            b = (void *)((uint64_t)(&b->bucket)[h].first & ~(1<<0));

            h = hash(n->data.key,b->header.n_buckets);

            old = bucket_lock;
            bucket_lock = &(&b->bucket)[h].lock_b;
            pthread_rwlock_wrlock(bucket_lock);
            pthread_rwlock_unlock(old);  
        }
        
        n->next = (&b->bucket)[h].first;
        (&b->bucket)[h].first = n;
        
        pthread_rwlock_unlock(bucket_lock);

        pthread_rwlock_wrlock(&b->header.lock);
        if (b->header.n_ele>-1) {
            b->header.n_ele++;
        }
        pthread_rwlock_unlock(&b->header.lock);
        return;
    }
    return;
}

//função do paper que ajusta os nodes -> passa o marking node para o fim de cada bucket. 
void adjustNodes(node* n, hashtable* b,hashtable* old, void* id_ptr,int* r_n_elem) {

    if (n != (void*)old && !is_bucket_array(n)) { 
        node* chain = n;
        if ( chain->next != (void*)old && chain->next != (void*)b && !is_bucket_array(chain->next)) {
            adjustNodes(chain->next,b,old,id_ptr,r_n_elem);
        }
        
        delete_search_insert(b,chain,2,r_n_elem,id_ptr);

        return;
    }
    return;
}


// função de search para verificar se a key já existe na hashtable
// faz uso da função de pesquisa 
// as locks pareciam estar erradas troquei a read para  a pos 0 e write para pos 1
int delete_search_insert(hashtable* b,node* n, int flag, int* r_n_elem, void* id_ptr){
    int h = hash(n->data.key,b->header.n_buckets);
    int (*locks[])(pthread_rwlock_t*) = {pthread_rwlock_rdlock,pthread_rwlock_wrlock,pthread_rwlock_wrlock};

    pthread_rwlock_t* bucket_lock = &(&b->bucket)[h].lock_b;
    
    locks[flag](bucket_lock);
    
    if((&b->bucket)[h].first == (void*)b) {
        if(flag!=2) {
            pthread_rwlock_unlock(bucket_lock);
            return 0;
        }
        else {
            n->next = (&b->bucket)[h].first;
            (&b->bucket)[h].first = n;
            locks[flag](&b->header.lock);
            if (b->header.n_ele>-1) {
                b->header.n_ele++;
            }
            pthread_rwlock_unlock(&b->header.lock);
            pthread_rwlock_unlock(bucket_lock);
            *r_n_elem = 1;
            return 1;
        }
    }

    while(is_bucket_array((&b->bucket)[h].first)) {

        b = (void *)((uint64_t)(&b->bucket)[h].first & ~(1<<0));

        h = hash(n->data.key,b->header.n_buckets);

        pthread_rwlock_unlock(bucket_lock);
        bucket_lock = &(&b->bucket)[h].lock_b;
        locks[flag](bucket_lock);
    }

    node* first = (&b->bucket)[h].first;
    node* prev = first;
    int count = 1;

    while(first != (void*)b) {
        if(first->data.key == n->data.key) {
            if(flag==0){
                pthread_rwlock_unlock(bucket_lock);  
                return 1;
            }
            else if (flag==1) {
                if(first == prev) {
                    (&b->bucket)[h].first = first->next;
                }
                else {
                    prev->next = first->next;
                }
                pthread_rwlock_unlock(bucket_lock);
                pthread_rwlock_wrlock(&b->header.lock);
                if (b->header.n_ele>0) {
                    b->header.n_ele--;
                }
                pthread_rwlock_unlock(&b->header.lock);
                return 1;
            }
        }
        count++;
        prev = first;
        first = first->next;
    }

    if(flag==2) {
        if(count==1) {
            n->next = (&b->bucket)[h].first;
            (&b->bucket)[h].first = n;
        }
        else {
            n->next = prev->next;
            prev->next = n;
        }
        pthread_rwlock_unlock(bucket_lock);
        pthread_rwlock_wrlock(&b->header.lock);
        if (b->header.n_ele>-1) {
            b->header.n_ele++;
        }
        pthread_rwlock_unlock(&b->header.lock);
        *r_n_elem = count;
        return 1;
    }

    pthread_rwlock_unlock(bucket_lock); 

    return 0;
}

// função de hash expansion
hashtable* HashExpansion(hashtable* b, void* id_ptr,int* r_n_elem) {
    hashtable* oldB = b;
    int oldK = b->header.n_buckets;
    int newK = 2*oldK;
    hashtable* newB = create_table(newK);
    int i=0;
    node* node_mask = (void*)(uint64_t*)((uint64_t)newB | 1);
    
    while (i < oldK) {

        //substituir o cas por um if
        pthread_rwlock_wrlock(&((&oldB->bucket)[i].lock_b));
        
        if ((&oldB->bucket)[i].first != (void*)b) { //&& (&oldB->bucket)[i].first != node_mask
            
            adjustNodes((&oldB->bucket)[i].first,newB,oldB,id_ptr,r_n_elem);
            
        }  

        (&oldB->bucket)[i].first = node_mask;
        pthread_rwlock_unlock(&((&oldB->bucket)[i].lock_b));

        i++;
    }

    return newB;
}

// função que faz handle do processo de insert
// e que dá inicio À expansão quando necessário
//os prints comentados nesta função são usados para fazer debuging. 
int main_hash(hashtable* b,int k, int v, void* id_ptr, int* r_n_elem, int flag) {

    node* n = inst_node(k,v,b);

    if(delete_search_insert(b,n,flag,r_n_elem,id_ptr)) {
        if(flag!=2) {
            return 1;
        }
    }
    else {
        return 0;
    }

    
    //quando uma thread insere uma key, se quando entrou o n_elem != -1 e quando sai é ==-1,
    //isto quer dizer que a table expandiu após a inserção, no mommento de expansão
    //à uma recontagem dos elementos à medida que são ajustados na nova tabela
    //logo, a thread não pode aumentar o nr de elementos visto que já foi contado pela expansão
    // se quando entrou o n_ele == -1, o mecanismo que lida com isso está na função de inserção 
    // se quando voltou o valor é !=-1, deve incrementar e verificar se precisa de ser expandida. 
    

    pthread_rwlock_wrlock(&b->header.lock);

    if((*r_n_elem > TRESH) && (b->header.n_ele > b->header.n_buckets)) { 

        b->header.n_ele = -1;
            pthread_rwlock_unlock(&b->header.lock);
            hashtable* oldb = b;
            b = HashExpansion(b,id_ptr,r_n_elem);
            
            pthread_rwlock_wrlock(&oldb->header.lock);
            oldb->header.n_ele = -2;
            pthread_rwlock_unlock(&oldb->header.lock);

            oldb = ht;
            pthread_rwlock_wrlock(&oldb->header.lock);
            if (ht->header.n_buckets < b->header.n_buckets) {
                ht = b;
            }
            pthread_rwlock_unlock(&oldb->header.lock);
            return 1;
    }

    pthread_rwlock_unlock(&b->header.lock);
    return 1;
}



//---------------------------------------------------------------------------------------------------------------




//search/insert/delete estão todos na mesma função e a funcionar
//é preciso: - mudar o nome das funções para fazer sentido - done
//           - mudar a maneira como os testes são criados para fazer hits e misses, e escrever no ficheiro como inicialmente falado numero - instrução - falta hit e miss
//           - usar o random para se o numero cair numa certa parte escolher a instrução - done
//           - fazer uma pré inserção dos números a procurar/apagar em sequencial para poder correr em paralelo as 3 operações em simultâneo - done
//           - verificar se posso alterar os read/write locks para nos searches serem read e nos inserts/delete write - done

// -- ver como medir o tempo de trabalho só das threads, lançar as threads por si sí demora 10x mais que em serial a concluir as operações na table. 


// preciso ver o numero dos cores e fazer pin as threads nos cores de performance 
// -- wrapper cpp no codigo para ter o seu proprio namespace para poder usar a biblioteca dos lock free locks
