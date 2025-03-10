//#include <lfht.h>
#include "expht.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
// search
// delete
// insert 2
#if DEBUG

#include <assert.h>

#endif

#define GOLD_RATIO 11400714819323198485ULL
#define LRAND_MAX (1ULL<<31)
#define INIT_SIZE 8


size_t	limit_sf,
    	limit_r,
    	limit_i;


int test_size,
    n_threads;

//struct lfht_head *head;
hashtable* ht ;


void *prepare_worker(void *entry_point)
{
    int* r_n_elem = (int*)calloc(sizeof(int),1);

	//int thread_id = lfht_init_thread(head);
	for(int i=0; i<test_size/n_threads; i++){
		size_t rng,
                value;
		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;
		if(rng < limit_r)
            // 		 ht, key, value, id_ptr, elem, instruction...
            main_hash(ht,value,value%test_size,entry_point,r_n_elem,2);
			//lfht_insert(head, value, (void*)value, thread_id);
	}
	//lfht_end_thread(head, thread_id);
	return NULL;
}

void *bench_worker(void *entry_point)
{
	//int thread_id = lfht_init_thread(head);
	int thread_limit = test_size/n_threads;
    int* r_n_elem = (int*)calloc(sizeof(int),1);

	for(int i=0; i<thread_limit; i++){
		size_t	rng,
		    	value;
		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;
		if(rng < limit_sf){

#if DEBUG
			//assert((size_t)lfht_search(head, value, thread_id)==value);
			assert(main_hash(ht,value,value%test_size,entry_point,r_n_elem,0)==1);
#else
			//lfht_search(head, value, thread_id);
            main_hash(ht,value,value%test_size,entry_point,r_n_elem,0);

#endif
		}
		else if(rng < limit_r){
			//lfht_remove(head, value, thread_id);
            main_hash(ht,value,value%test_size,entry_point,r_n_elem,1);

		}
		else if(rng < limit_i){
			//lfht_insert(head, value, (void*)value, thread_id);
            main_hash(ht,value,value%test_size,entry_point,r_n_elem,2);

		}
		else{
#if DEBUG
			//assert(lfht_search(head, value, thread_id) == NULL);
			assert(main_hash(ht,value,value%test_size,entry_point,r_n_elem,0)==0);
#else
			//lfht_search(head, value, thread_id);
            main_hash(ht,value,value%test_size,entry_point,r_n_elem,0);
#endif
		}
	}
	//lfht_end_thread(head, thread_id);
	return NULL;
}
#if DEBUG
void *test_worker(void *entry_point)
{
	//int thread_id = lfht_init_thread(head);
    int* r_n_elem = (int*)calloc(sizeof(int),1);

	for(int i=0; i<test_size/n_threads; i++){
		size_t rng,
		       value;
		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;
		if(rng < limit_sf){
			assert(main_hash(ht,value,value%test_size,entry_point,r_n_elem,0)==1);
		}
		else if(rng < limit_r){
			assert(main_hash(ht,value,value%test_size,entry_point,r_n_elem,0)==0);
		}
		else if(rng < limit_i){
			assert(main_hash(ht,value,value%test_size,entry_point,r_n_elem,0)==1);
		}
		else{
			assert(main_hash(ht,value,value%test_size,entry_point,r_n_elem,0)==0);
		}
	}
	//lfht_end_thread(head, thread_id);
	return NULL;
}
#endif

int main(int argc, char **argv)
{
	if(argc < 7){
		printf("usage: bench <threads> <nodes> <inserts> <removes> <searches found> <searches not found>\n");
		return -1;
	}
	
	printf("preparing data.\n");
	
	n_threads = atoi(argv[1]);
	test_size = atoi(argv[2]);
	
	size_t	inserts = atoi(argv[3]),
	    	removes = atoi(argv[4]),
	    	searches_found = atoi(argv[5]),
	    	searches_not_found = atoi(argv[6]),
	    	total = inserts + removes + searches_found + searches_not_found;
	
			limit_sf = LRAND_MAX*searches_found/total;
	
	limit_r = limit_sf + LRAND_MAX*removes/total;
	limit_i = limit_r + LRAND_MAX*inserts/total;
	
	struct timespec start_monoraw,
					end_monoraw,
					start_process,
					end_process;
	double time;
	
	pthread_t *threads = malloc(n_threads*sizeof(pthread_t));
	
	struct drand48_data **seed = malloc(n_threads*sizeof(struct drand48_data *));

	//head = init_lfht(n_threads);
    ht = create_table(INIT_SIZE);

	
	for(int i=0; i<n_threads; i++)
		seed[i] = aligned_alloc(64, 64);
	
	
	if(limit_r!=0){
		for(int i=0; i<n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, prepare_worker, seed[i]);
		}
		for(int i=0;i<n_threads; i++){
			pthread_join(threads[i], NULL);
		}
	}
	
	printf("starting test\n");
	
	//imprimir_hash(ht);

	clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
	
	for(int i=0; i<n_threads; i++){
		srand48_r(i, seed[i]);
		pthread_create(&threads[i], NULL, bench_worker, seed[i]);
	}
	
	for(int i=0; i<n_threads; i++)
		pthread_join(threads[i], NULL);
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);
	
	time = end_monoraw.tv_sec - start_monoraw.tv_sec + ((end_monoraw.tv_nsec - start_monoraw.tv_nsec)/1000000000.0);
	printf("Real time: %lf\n", time);
	
	time = end_process.tv_sec - start_process.tv_sec + ((end_process.tv_nsec - start_process.tv_nsec)/1000000000.0);
	printf("Process time: %lf\n", time);

	//imprimir_hash(ht);

#if DEBUG
	for(int i=0; i<n_threads; i++){
		srand48_r(i, seed[i]);
		pthread_create(&threads[i], NULL, test_worker, seed[i]);
	}
	for(int i=0; i<n_threads; i++){
		pthread_join(threads[i], NULL);
	}
	printf("Correct!\n");
#endif
	return 0;
}
