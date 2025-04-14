//#include <lfht.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <pthread.h>
#include <time.h>

// version flag = 3 -> d
// version flag = 2 -> c
// version flag = 1 -> b
// version flag = 0 -> a

#if DEBUG

#include <assert.h>

#endif

#if VERSION == 1
	#include "expht_b.h"
#elif VERSION == 2
	#include "expht_c.h"
#elif VERSION == 3 
	#include "expht_d.h"
#elif VERSION == 4
	#include "expht_e.h"
#elif VERSION == 5
	#include "expht_f.h"
#else 
	#include "expht_a.h" 
#endif

#define GOLD_RATIO 11400714819323198485ULL
#define LRAND_MAX (1ULL<<31)
#define INIT_SIZE 16384		// (2^14) initial size of hashtable to avoid thread colision -- must be base 2 for hashing


size_t	limit_sf,
    	limit_r,
    	limit_i;

int test_size,
    n_threads;

//struct lfht_head *head;
access* entry ;


void *prepare_worker(void *entry_point)
{

	int64_t thread_id = get_thread_id(entry);

	//int thread_id = lfht_init_thread(head);
	//printf("test size: %d # threads: %d\n",test_size,n_threads);
	for(int i=0; i<test_size/n_threads; i++){
		size_t rng,
                value;
		//isto aqui estava como long int
		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;
		//printf("value: %d table size: %d \n",value,entry->ht->header.n_buckets);
		if(rng < limit_r)
            // 		 ht, key, value, id_ptr, elem, instruction...
			main_hash(entry,value,thread_id);
			//lfht_insert(head, value, (void*)value, thread_id);
	}
	//lfht_end_thread(head, thread_id);
	return NULL;
}

void *bench_worker(void *entry_point)
{

	//int thread_id = lfht_init_thread(head);
	int thread_limit = test_size/n_threads;

	int64_t thread_id = get_thread_id(entry);

	for(int i=0; i<thread_limit; i++){
		size_t	rng,
		    	value;
		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;
		//printf("value: %llu rng: %llu limit_sf: %llu limit_r: %llu limit_i: %llu \n", value, rng, limit_sf, limit_r, limit_i);
		if(rng < limit_sf){

#if DEBUG
//			assert(search(entry->ht,value,entry_point)==1);

			assert(search(entry,value,thread_id)==1);
#else

			//search(entry->ht,value,thread_id);

			search(entry,value,thread_id);

#endif
		}
		else if(rng < limit_r){
			//lfht_remove(head, value, thread_id);
            //main_hash(ht,value,value%test_size,thread_id,r_n_elem,1);
			//delete(entry->ht,value,thread_id);
			delete(entry,value,thread_id);
		}
		else if(rng < limit_i){
			main_hash(entry,value,thread_id);
		}
		else{
#if DEBUG

			assert(search(entry,value,thread_id)==0);

#else

			search(entry,value,thread_id);

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

	int64_t thread_id = get_thread_id(entry);

	for(int64_t i=0; i<test_size/n_threads; i++){
		size_t	rng, value;

		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;
		if(rng < limit_sf){
			//hit
			//assert(search(entry->ht,value,thread_id)==1);

			assert(search(entry,value,thread_id)==1);
		}
		else if(rng < limit_r){
			//delete
			assert(search(entry,value,thread_id)==0);
		}
		else if(rng < limit_i){
			//insert
			assert(search(entry,value,thread_id)==1);
		}
		else{
			//miss
			assert(search(entry,value,thread_id)==0);
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


	entry = create_acess(INIT_SIZE,n_threads);

	//printf("Table created\n");
	
	for(int64_t i=0; i<n_threads; i++)
		seed[i] = aligned_alloc(64, 64);
	
	//printf("seeded \n");

	if(limit_r!=0){
		for(int64_t i=0; i<n_threads; i++){
			srand48_r(i, seed[i]);
			pthread_create(&threads[i], NULL, prepare_worker, seed[i]);
		}
		for(int64_t i=0;i<n_threads; i++){
			pthread_join(threads[i], NULL);
		}
	}
	
	printf("starting test\n");
	entry->header.thread_id = 0;
	
	//imprimir_hash(ht);

	clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
	
	for(int64_t i=0; i<n_threads; i++){
		srand48_r(i, seed[i]);
		pthread_create(&threads[i], NULL, bench_worker, seed[i]);
	}
	
	for(int64_t i=0; i<n_threads; i++)
		pthread_join(threads[i], NULL);
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);
	
	time = end_monoraw.tv_sec - start_monoraw.tv_sec + ((end_monoraw.tv_nsec - start_monoraw.tv_nsec)/1000000000.0);
	printf("Real time: %lf\n", time);
	
	time = end_process.tv_sec - start_process.tv_sec + ((end_process.tv_nsec - start_process.tv_nsec)/1000000000.0);
	printf("Process time: %lf\n", time);

	//imprimir_hash(entry->ht);

#if DEBUG

	entry->header.thread_id = 0;

	for(int64_t i=0; i<n_threads; i++){
		srand48_r(i, seed[i]);
		pthread_create(&threads[i], NULL, test_worker, seed[i]);
	}
	for(int64_t i=0; i<n_threads; i++){
		pthread_join(threads[i], NULL);
	}
	printf("Correct!\n");
#endif
	return 0;
}
