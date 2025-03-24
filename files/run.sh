#! /usr/bin/env bash
#./run.sh LOCK N_TEST N_ELEM

LOCK=$1
N_TEST=$2
N_ELEM=$3

#add 1 test, 1st test is always thrown out
N_TEST=$(( $N_TEST + 1 ))


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
MAX_TH=65

while [ "$N_TH" -lt "$MAX_TH" ]
do
    FILE="../tests/${N_TH}_${N_ELEM}_${LOCK}.csv"
    printf "inserts real time, inserts process time, deletes real time, deletes process time, hits real time, hits process time, misses real time, misses process time, mixed real time, mixed process time\n" > $FILE
    
    B=0;

    while [ "$B" -lt "$N_TEST" ]
    do
    #printf "Test %d: #Threads: %d - All inserts \n" $A $N_TH # | tee log.txt    

    #pegar no output e tirar os tempos
    printf "Test %d: #Threads: %d Lock: %d " $B $N_TH $LOCK # | tee log.txt   
    
    #overwrite file then add the others to file
    printf "testing all inserts..."
    ./bench_debug $N_TH $N_ELEM 100 0 0 0  > ../tests/times.txt
    printf "testing all deletes..."
    ./bench_debug $N_TH $N_ELEM 0 100 0 0  >> ../tests/times.txt
    printf "testing all hits..."
    ./bench_debug $N_TH $N_ELEM 0 0 100 0  >> ../tests/times.txt
    printf "testing all misses..."
    ./bench_debug $N_TH $N_ELEM 0 0 0 100  >> ../tests/times.txt
    printf "testing mixed...\n"
    ./bench_debug $N_TH $N_ELEM 50 20 20 10  >> ../tests/times.txt


    #throw away 1st test
    if [ 0 -lt "$B" ]; then
        # get only numbers -> take empty lines out -> put comma in every line that isn't the last line -> replace \n with a space -> output to csv file
        sed  "s/[^0-9].[^0-9]*//" ../tests/times.txt  | sed '/^\s*$/d' | sed '$!s/$/,/' | tr "\n" " " >> $FILE
        printf "\n" >> $FILE
    fi 

    B=$(( $B + 1 ))
    done 

    #N_TH=$(( $N_TH + $N_TH ))
    N_TH=$(( $N_TH + 1 ))
done 