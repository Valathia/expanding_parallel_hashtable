#! /usr/bin/env bash

LOCK=$1
N_TEST=$2


if [ $LOCK -ne 0 ]
then
    #MUTEX 
    gcc -c expht.c -DMUTEX=1 -o expht.o
else
    #READWRITE
    gcc -c expht.c -o expht.o
fi

ar rcu libexpht.a expht.o
gcc bench_.c libexpht.a -g -ggdb -Og -DDEBUG=1 -lpthread -o bench_debug


N_TH=1
MAX_TH=33

while [ "$N_TH" -lt "$MAX_TH" ]
do
A=0
while [ "$A" -lt "$N_TEST" ]
do
    printf "Test %d: #Threads: %d - All inserts \n" $A $N_TH # | tee log.txt    
    ./bench_debug $N_TH 1000000 100 0 0 0 # >> ../tests/batch_test.txt
    printf "Test %d: #Threads: %d - All deletes \n" $A $N_TH # | tee log.txt    
    ./bench_debug $N_TH 1000000 0 100 0 0 # >> ../tests/batch_test.txt
    printf "Test %d: #Threads: %d - All hits \n" $A $N_TH # | tee log.txt    
    ./bench_debug $N_TH 1000000 0 0 100 0 # >> ../tests/batch_test.txt
    printf "Test %d: #Threads: %d - All misses \n" $A $N_TH # | tee log.txt    
    ./bench_debug $N_TH 1000000 0 0 0 100 # >> ../tests/batch_test.txt
    printf "Test %d: #Threads: %d - Mixed \n" $A $N_TH # | tee log.txt    
    ./bench_debug $N_TH 1000000 50 20 20 10 # >> ../tests/batch_test.txt

    A=$(( $A + 1 ))
done

    N_TH=$(( $N_TH + $N_TH ))
done 