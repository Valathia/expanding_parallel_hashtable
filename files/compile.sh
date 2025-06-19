#!/bin/bash


#compila o array
gcc -c -Wshadow files_xato/expht_array.c -DMUTEX=1 -DTRESH=2 -DRATIO=1 -DARRAY=1 -o files_xato/expht_array.o -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
ar rcu files_xato/libexpht_array.a files_xato/expht_array.o
gcc -Wshadow files_xato/bench_.c files_xato/libexpht_array.a -DMUTEX=1 -DTRESH=2 -DRATIO=1 -DARRAY=1 -DTIMED=1  -lpthread -o files_xato/bench_array -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc

#compila a lista
gcc -c -Wshadow files_xato/expht_list.c -DMUTEX=1 -DTRESH=2 -DRATIO=1 -o files_xato/expht_list.o -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
ar rcu files_xato/libexpht_list.a files_xato/expht_list.o
gcc -Wshadow files_xato/bench_.c files_xato/libexpht_list.a -DMUTEX=1 -DTRESH=2 -DRATIO=1 -DTIMED=1  -lpthread -o files_xato/bench_list -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc

#compila os setups
gcc -Wshadow files_xato/bench_.c files_xato/libexpht_list.a -DMUTEX=1  -DTRESH=2 -DRATIO=1 -DTIMED=1 -DSETUP=1 -lpthread -o files_xato/bench_list_setup -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
gcc -Wshadow files_xato/bench_.c files_xato/libexpht_array.a -DMUTEX=1  -DTRESH=2 -DRATIO=1 -DARRAY=1 -DTIMED=1 -DSETUP=1  -lpthread -o files_xato/bench_array_setup -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc

#comandos com o perf
#perf stat -a -e power/energy-pkg/,cache-misses,cache-references,L1-dcache-load-misses,L1-icache-load-misses,LLC-load-misses,instructions,alignment-faults files_xato/bench_array 16 4194304 0 0 100 0 
#perf stat -a -e power/energy-pkg/,cache-misses,cache-references,L1-dcache-load-misses,L1-icache-load-misses,LLC-load-misses,instructions,alignment-faults files_xato/bench_list 16 4194304 0 0 100 0 

# time perf stat -a -e power/energy-pkg/,cache-misses,cache-references,L1-dcache-load-misses,LLC-load-misses files_xato/bench_array_setup 16 4194304 0 0 100 0 

#correr 100 searches array, 16 threads , 4194304 elementos
# files_xato/bench_array 16 4194304 0 0 100 0 

# files_xato/bench_list 16 1048576 0 0 100 0 

#T::2 20 10 10 80 

perf stat -a -e power/energy-pkg/,cache-misses,cache-references,L1-dcache-load-misses,L1-icache-load-misses,LLC-load-misses,instructions,alignment-faults files_xato/bench_list 16 1048576 0 0 100 0 
perf stat -a -e power/energy-pkg/,cache-misses,cache-references,L1-dcache-load-misses,L1-icache-load-misses,LLC-load-misses,instructions,alignment-faults files_xato/bench_array 16 1048576 0 0 100 0 
