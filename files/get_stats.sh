#!/bin/bash

OPSHEADER="#Threads, Time, Process Time, Total Ops, Ops/Second, HT Size, Load Factor, Elements Recorded, Elements Total, Elements Actual, Most Elements in Chain, Least Elements in Chain, Largest Bucket Size, Smallest Bucket Size, # Chains at Largest Size, # Chains at Smallest Size, Average Bucket Size \n"
DEBUG=0
DEBUG_FLAG=""

LOCK="-DMUTEX=1"

while getopts ":d" opt; do
    case $opt in 
        d)
            printf "debug mode\n"
            DEBUG_FLAG="-g -ggdb -Og -DDEBUG=1"
            DEBUG=1
            ;;
        ?)
            echo "Invalid option: -$OPTARG" >&2
            exit 1
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            exit 1
            ;;
    esac
done

N_TEST=${@:$OPTIND:1}
N_TEST=$(( $N_TEST + 1 ))

VERSION=${@:$OPTIND+1:1}
NELEM=${@:$OPTIND+2:1}
TRESH=${@:$OPTIND+3:1}
LOADFACTOR=$(( $TRESH - 1 ))

gcc -c -Wshadow files/expht_${VERSION}.c ${LOCK} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -o files/expht.o -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
ar rcu files/libexpht.a files/expht.o

case $VERSION in
    list)
        printf "picked version list\n"
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -lpthread -o files/bench_list -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -DSETUP=1 -lpthread -o files/bench_setup -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc

    ;;
    array)
        printf "picked version array\n"
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -DARRAY=1 -lpthread -o files/bench_array -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -DARRAY=1 -DSETUP=1 -lpthread -o files/bench_setup -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
    ;;
    *)
        printf "I need to know which version of the struct you wanna use mate \n"
        exit 1
    ;;
esac

THREADS=(1 2 4 8 12 16 20)
TESTS=()
TESTS+=(100 0 0 0)


T=0;
MAXT=4;
while [ "$T" -lt "$MAXT" ]
do
    INSERTS=${TESTS[$T]}
    T=$(( $T + 1 ))

    DELETES=${TESTS[$T]}
    T=$(( $T + 1 ))

    SEARCHF=${TESTS[$T]}
    T=$(( $T + 1 ))

    SEARCHNF=${TESTS[$T]}
    T=$(( $T + 1 ))

    if [ "$DEBUG" -eq 1 ]
    then 
        printf "Printing to debug folder \n"
        FILE="tests/debug/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}_stats.csv"
    else 
        FILEOPS="tests/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}_stats.csv"
    fi

    printf "Testing: struct ${VERSION} Tresh:${TRESH} Ratios: ${INSERTS} ${DELETES} ${SEARCHF} ${SEARCHNF} \n"

    printf "${OPSHEADER}" > $FILEOPS

    for N_TH in  "${THREADS[@]}"
    do

        B=0;

        while [ "$B" -lt "$N_TEST" ]
        do
        #printf "Test %d: #Threads: %d - All inserts \n" $A $N_TH # | tee log.txt    

        #pegar no output e tirar os tempos
        printf "Test %d: #Threads: %d ...\n" $B $N_TH # | tee log.txt   
        

        #overwrite file then add the others to file
        printf "Testing File... \n"
        files/bench_$VERSION $N_TH $NELEM $INSERTS $DELETES $SEARCHF $SEARCHNF  > tests/times.txt

        #throw away 1st test
        if [ 0 -lt "$B" ]; then
            # get only numbers -> take empty lines out -> -> replace \n with a space -> output to csv file
            printf "${N_TH}, " >> $FILEOPS
            sed  "s/[^0-9].[^0-9]*//" tests/times.txt  | sed '/^\s*$/d' | sed '$!s/$/,/' | tr "\n" " " >> $FILEOPS
            printf "\n" >> $FILEOPS

        fi 

        B=$(( $B + 1 ))
        done 
    done
done
