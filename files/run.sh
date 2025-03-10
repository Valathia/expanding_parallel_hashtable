#! /usr/bin/env bash
./compile_debug.sh
N_TEST=$1
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

# printf "1 Thread Test\n"
# ./bench_debug 1 100000 50 20 20 10 >> ../tests/test1_100000_debug.txt

# printf "2 Thread Test\n"
# ./bench_debug 2 100000 50 20 20 10 >> ../tests/test2_100000_debug.txt

# printf "4 Thread test \n"
# ./bench_debug 4 100000 50 20 20 10 >> ../tests/test4_100000_debug.txt

# printf "8 Thread test \n"
# ./bench_debug 8 100000 50 20 20 10 >> ../tests/test8_100000_debug.txt

# printf "12 Thread test \n"
# ./bench_debug 12 100000 50 20 20 10 >> ../tests/test12_100000_debug.txt