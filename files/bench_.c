//#include <lfht.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <pthread.h>
#include <time.h>

#if DEBUG
#include <assert.h>
#endif

#include "config.h"

#define GOLD_RATIO 11400714819323198485ULL
#define LRAND_MAX (1ULL<<31)
#define INIT_SIZE 16384		// (2^14) initial size of hashtable to avoid thread colision -- must be base 2 for hashing


size_t	limit_sf,
    	limit_r,
    	limit_i;

int test_size,
    n_threads;

access* entry ;


void *prepare_worker(void *entry_point)
{

	int64_t thread_id = get_thread_id(entry);

	for(int i=0; i<test_size/n_threads; i++){
		size_t rng,
                value;

				lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;

		if(rng < limit_r)
            // 		 ht, key, value, id_ptr, elem, instruction...
			main_hash(entry,value,thread_id);
	}
	//lfht_end_thread(head, thread_id);
	return NULL;
}

void *bench_worker(void *entry_point)
{

	int thread_limit = test_size/n_threads;

	int64_t thread_id = get_thread_id(entry);

	for(int i=0; i<thread_limit; i++){
		size_t	rng,
		    	value;
		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_RATIO;

		if(rng < limit_sf){
			//search find
			#if DEBUG
				int res =search(entry,value,thread_id);
				if(!res) {
					FILE* f = fopen("test.txt","w");
					fprintf(f,"Couldn't Find Value: %zu \n",value);
					imprimir_hash(entry->ht,f);
					fclose(f);
				}
				
				assert(res==1);
			#else
						search(entry,value,thread_id);
			#endif
		}
		else if(rng < limit_r){
			//remove
			delete(entry,value,thread_id);
		}
		else if(rng < limit_i){
			//insert
			main_hash(entry,value,thread_id);
		}
		else{
			//search miss
			#if DEBUG
				assert(search(entry,value,thread_id)==0);
			#else
				search(entry,value,thread_id);
			#endif
		}
		entry->header.insert_count[thread_id].all_ops++;
	}
	//lfht_end_thread(head, thread_id);
	return NULL;
}
#if DEBUG
void *test_worker(void *entry_point)
{

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

//if all inserts
void stats() {

	hashtable* ht = entry->ht;
	int64_t table_size = ht->header.n_buckets;
	int64_t recorded_elements = ht->header.n_ele;
	int64_t total_elements = ht->header.n_ele;
	int64_t list_size = 0;
	int64_t max_size = 0;
	int64_t min_size = INT32_MAX;
	int64_t aux = 0;
	int64_t aux_exp = 0;
	int64_t min_exp = INT32_MAX;
	int64_t max_exp = 0;
	int64_t exp_size = 0;
	int64_t header_access = 0;
	//n_elementos
	int64_t moda[128];
	for(int64_t i=0; i<128;i++) {
		moda[i] = 0;
	}

	for(int64_t i=1; i<n_threads+1; i++) {
		//printf("Thread %llu : Inserted %llu items - Last Known Hashtable: %d \n",i,entry->header.insert_count[i].count,entry->header.insert_count[i].header);	
		if(entry->header.insert_count[i].header == entry->ht->header.n_buckets) {
			total_elements += entry->header.insert_count[i].count;
		}
		header_access += entry->header.insert_count[i].ht_header_lock_count;
	}
	
	#ifdef ARRAY
		for(int64_t i=0; i<table_size; i++) {
			aux = (&ht->bucket)[i].n;
			aux_exp = (&ht->bucket)[i].size;
			moda[aux] += 1;

			if (aux > 0) {
				if(aux > max_size) {
					max_size = aux;
				}

				if(aux < min_size) {
					min_size = aux;
				}
				
				list_size+=aux;
			}
			

			if(aux_exp > max_exp) {
				max_exp = aux_exp;
			}

			if(aux < min_exp) {
				min_exp = aux_exp;
			}
			
			exp_size+=aux_exp;
			
		}
	#else 
		for(int64_t i=0; i<table_size; i++) {
			aux = bucket_size((&ht->bucket)[i].first,ht);
			moda[aux] += 1;
			if (aux > 0) {
				if(aux > max_size) {
					max_size = aux;
				}

				if(aux < min_size) {
					min_size = aux;
				}
				
				list_size+=aux;
			}
		}
		max_exp = max_size;
		min_exp = min_size;
	#endif
	int64_t max_mode = moda[0];
	int64_t max_i = 0;
	int64_t min_mode = moda[0];
	int64_t min_i = 0;
	for(int64_t i=0; i<64;i++) {
		if(moda[i] > max_mode) {
			max_mode = moda[i];
			max_i = i; 
		}

		if(moda[i] > min_mode) {
			min_mode = moda[i];
			min_i = i; 
		}
	}

	printf("Hashtable size: %ld\n", table_size);
	printf("Elements Recorded: %ld\n", recorded_elements);
	printf("Elements Total: %ld\n", total_elements);
	printf("Elements Actual: %ld\n",list_size);
	printf("Largest Element Size: %ld\n",max_size);
	printf("Smallest Element Size: %ld\n",min_size);
	printf("Load Factor: %lf\n",(float)list_size/(float)table_size);
	printf("Largest Bucket Size: %ld\n",max_exp);
	printf("Smallest Bucket Size: %ld\n",min_exp);
	printf("# Buckets at Largest Size: %ld\n",moda[max_size]);
	printf("# Buckets at Smallest Size: %ld\n",moda[min_size] + moda[0]);	
	printf("Average Bucket Size: %lf\n", (float)exp_size/(float)table_size);
	printf("Header Access Total: %ld\n",header_access);
	printf("Header Access Average: %lf\n",(float)header_access/(float)n_threads);
}


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

	for(int64_t i=0; i<n_threads; i++)
		seed[i] = aligned_alloc(64, 64);
	
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
	entry->header.thread_id = (int64_t)1;
	
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

	int64_t total_ops = 0;

	for(int64_t i=1; i<n_threads+1; i++) {
		total_ops += entry->header.insert_count[i].all_ops;
	}
	printf("Total Operations: %ld\n",total_ops);


	if(inserts == 100) {
		stats();
	}
	//imprimir_hash(entry->ht);

#if DEBUG

	entry->header.thread_id = (int64_t)1;

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
