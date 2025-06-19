#include "config.h"
#include <time.h>
#include <signal.h>
#include <unistd.h>

#if DEBUG
#include <assert.h>
#endif


#define GOLD_LOADFACTOR 11400714819323198485ULL
#define LRAND_MAX (1ULL<<31)
#define INIT_SIZE 16384		// (2^14) initial size of hashtable to avoid thread colision -- must be base 2 for hashing


size_t	limit_sf,
    	limit_r,
    	limit_i;

int test_size,
    n_threads;

int START_BENCH = 0;

support* entry ;


void *prepare_worker(void *entry_point)
{

	int64_t thread_id = get_thread_id(entry);

	for(int i=0; i<test_size/n_threads; i++){
		size_t rng,
                value;

				lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_LOADFACTOR;

		if(rng < limit_r)
        	main_hash(entry,value,thread_id); 	//ht, key, value, id_ptr, elem, instruction...
	}

	return NULL;
}

void *bench_worker(void *entry_point)
{

	int thread_limit = test_size/n_threads;

	int64_t thread_id = get_thread_id(entry);

	while(!START_BENCH){};

	for(int i=0; i<thread_limit; i++){
		size_t	rng,
		    	value;
		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_LOADFACTOR;

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
		
		if(START_BENCH) {
			entry->header.insert_count[thread_id].all_ops++;
		}
		else {
			return NULL;
		}
		
	}

	return NULL;
}


#if DEBUG
void *test_worker(void *entry_point)
{

	int64_t thread_id = get_thread_id(entry);

	for(int64_t i=0; i<test_size/n_threads; i++){
		size_t	rng, value;

		lrand48_r(entry_point, (long int *) &rng);
		value = rng * GOLD_LOADFACTOR;
		if(rng < limit_sf){
			//hit

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
	//n_elementos
	int64_t moda[128];
	for(int64_t i=0; i<128;i++) {
		moda[i] = 0;
	}

	for(int64_t i=1; i<n_threads+1; i++) {

		if(entry->header.insert_count[i].header == entry->ht->header.n_buckets) {
			total_elements += entry->header.insert_count[i].count;
		}
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
	for(int64_t i=0; i<128;i++) {
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
	printf("Load Factor: %lf\n",(float)list_size/(float)table_size);
	printf("Elements Recorded: %ld\n", recorded_elements);
	printf("Elements Total: %ld\n", total_elements);
	printf("Elements Actual: %ld\n",list_size);
	printf("Most Elements in Chain: %ld\n",max_size); //alterar no script e no python, Largest e Smallest Element Size
	printf("Least Elements in Chain: %ld\n",min_size); //alterar no script e no python, Largest e Smallest Element Size

	
	printf("Largest Bucket Size: %ld\n",max_exp);

	if(min_exp < INT32_MAX) {	
		printf("Smallest Bucket Size: %ld\n",min_exp);
	}
	else {
		printf("Smallest Bucket Size: 0\n");
	}

	printf("# Chains at Largest Size: %ld\n",moda[max_size]);
	
	if(min_size < INT32_MAX) {
		printf("# Chains at Smallest Size: %ld\n",moda[min_size] + moda[0]);	
	}
	else {
		printf("# Chains at Smallest Size: %ld\n",moda[0]);	
	}
	
	printf("Average Bucket Size: %lf\n", (float)exp_size/(float)table_size);

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

	
	size_t	inserts =  atoi(argv[3]),
	    	removes =  atoi(argv[4]),
	    	searches_found = atoi(argv[5]),
	    	searches_not_found =  atoi(argv[6]),
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
	
		
	//"Ana porque é que puseste aqui uma função? " Pq se fizer só reset aos ids, o compilador ignora-me. (ノ-_-)ノ ミ ┴┴ 1 HORA NISTO --- confirma-se que eram optimizações da flag -O3
	//entry->header.thread_id = (int64_t)1;
	reset_id_counter(entry);

	for(int64_t i=0; i<n_threads; i++){
		srand48_r(i, seed[i]);
		pthread_create(&threads[i], NULL, bench_worker, seed[i]);
	}
	
	while (entry->header.thread_id < n_threads+1) {};

	#ifdef SETUP
		return 0;
	#endif
	printf("starting test\n");

	clock_gettime(CLOCK_MONOTONIC_RAW, &start_monoraw);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_process);
	
	//se usar as optimizações este START_BENCH não é actualizado
	START_BENCH=1;

	#if TIMED

		int64_t maxops = test_size/n_threads-1;
		int64_t j = 1;
		//wait for first thread to finish.  
		while(entry->header.insert_count[j].all_ops<maxops) {
			j++;
			if(j>n_threads) { 
				j=1;
			}
		}

		//kill all threads when 1st thread finishes
		START_BENCH = 0;

		clock_gettime(CLOCK_MONOTONIC_RAW, &end_monoraw);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_process);
		
		for(int64_t i=0; i<n_threads; i++) {
			pthread_join(threads[i], NULL);
		}
		
	#else
		for(int64_t i=0; i<n_threads; i++)
			pthread_join(threads[i], NULL);
	#endif 
	
	double real_time;
	time = end_monoraw.tv_sec - start_monoraw.tv_sec + ((end_monoraw.tv_nsec - start_monoraw.tv_nsec)/1000000000.0);
	real_time = time;
	printf("Real time: %lf\n", time);
	
	time = end_process.tv_sec - start_process.tv_sec + ((end_process.tv_nsec - start_process.tv_nsec)/1000000000.0);
	printf("Process time: %lf\n", time);

	int64_t total_ops = 0;

	for(int64_t i=1; i<n_threads+1; i++) {
		total_ops += entry->header.insert_count[i].all_ops;
	}
	printf("Total Operations: %ld\n",total_ops);

	printf("Ops per Second: %lf\n",total_ops/real_time);

	#if !TIMED
		if(inserts == 100) {
			stats();
		}
	#endif

#if DEBUG
	reset_id_counter(entry);

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
