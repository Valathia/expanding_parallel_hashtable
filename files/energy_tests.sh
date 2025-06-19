#!/bin/bash

# como correr:
# ./files/energy_tests.sh N_TESTS VERSION N_ELEM THRESH
#ou:  -> ver ficheiro run_energy.sh

OPSHEADER="#Threads, Time, Process Time, Total Ops, Ops/Second \n"
ENERGYHEADER="#Threads, Energy, Cache Misses, Cache References, L1-Cache Data Load Misses, LLC Load Misses \n"
DEBUG=0
DEBUG_FLAG=""

LOCK="-DMUTEX=1"

TIMED_FLAG="-DTIMED=1"

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
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} ${TIMED_FLAG} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -lpthread -o files/bench_list -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} ${TIMED_FLAG} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -DSETUP=1 -lpthread -o files/bench_setup -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc

    ;;
    array)
        printf "picked version array\n"
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} ${TIMED_FLAG} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -DARRAY=1 -lpthread -o files/bench_array -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
        gcc -Wshadow files/bench_.c files/libexpht.a ${DEBUG_FLAG} ${LOCK} ${TIMED_FLAG} -DTRESH=${TRESH} -DLOADFACTOR=${LOADFACTOR} -DARRAY=1 -DSETUP=1 -lpthread -o files/bench_setup -I$HOME/.local/include -L$HOME/.local/lib -ljemalloc
    ;;
    *)
        printf "I need to know which version of the struct you wanna use mate \n"
        exit 1
    ;;
esac

THREADS=(1 2 4 8 12 16 20)
TESTS=()
TESTS+=(100 0 0 0)
TESTS+=(0 100 0 0)
TESTS+=(0 0 100 0) 
TESTS+=(10 10 80 0)
TESTS+=(20 20 60 0)
TESTS+=(30 30 40 0)

T=0;
MAXT=24;
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
        FILE="tests/debug/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}.csv"
        FILEEN="tests/debug/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}_energy.csv"
        FILEBASE="tests/debug/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}_setup.csv"
    else 
        FILEOPS="tests/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}.csv"
        FILEEN="tests/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}_energy.csv"
        FILEBASE="tests/${VERSION}_${NELEM}_${TRESH}_${INSERTS}_${DELETES}_${SEARCHF}_${SEARCHNF}_setup.csv"
    fi

    printf "Testing: struct ${VERSION} Tresh:${TRESH} Ratios: ${INSERTS} ${DELETES} ${SEARCHF} ${SEARCHNF} \n"

    printf "${OPSHEADER}" > $FILEOPS
    printf "${ENERGYHEADER}" > $FILEEN
    printf "${ENERGYHEADER}" > $FILEBASE


    for N_TH in  "${THREADS[@]}"
    do

        B=0;

        while [ "$B" -lt "$N_TEST" ]
        do

        #pegar no output e tirar os tempos
        printf "Test %d: #Threads: %d ...\n" $B $N_TH # | tee log.txt   
        

        #overwrite file then add the others to file
        printf "Testing Setup... "
        perf stat -o tests/energy_setup.txt -a -e power/energy-pkg/,cache-misses,cache-references,L1-dcache-load-misses,LLC-load-misses files/bench_setup $N_TH $NELEM $INSERTS $DELETES $SEARCHF $SEARCHNF  > tests/times_setup.txt
        printf "Testing File... \n"
        perf stat -o tests/energy.txt -a -e power/energy-pkg/,cache-misses,cache-references,L1-dcache-load-misses,LLC-load-misses files/bench_$VERSION $N_TH $NELEM $INSERTS $DELETES $SEARCHF $SEARCHNF  > tests/times.txt

        #throw away 1st test
        if [ 0 -lt "$B" ]; then
            # get only numbers -> take empty lines out -> add commas at end of lines -> replace \n with a space -> output to csv file
            printf "${N_TH}, " >> $FILEOPS
            sed  "s/[^0-9].[^0-9]*//" tests/times.txt  | sed '/^\s*$/d' | sed '$!s/$/,/' | tr "\n" " " >> $FILEOPS
            printf "\n" >> $FILEOPS
            
            # get only numbers -> take empty lines out -> take out 1st line -> take out last line -> Get first column -> add commas at end of lines  -> replace \n with a space -> output to csv file
            printf "${N_TH}, " >> $FILEEN
            sed  "s/[^0-9].[^0-9]*//" tests/energy.txt |  sed '/^\s*$/d' | tail -n +2 | head -n -1 | cut -d ' ' -f1 | sed "s/,//g" | sed '$!s/$/,/' | tr "\n" " " >> $FILEEN
            printf "\n" >> $FILEEN

            printf "${N_TH}, " >> $FILEBASE
            sed  "s/[^0-9].[^0-9]*//" tests/energy_setup.txt |  sed '/^\s*$/d' | tail -n +2 | head -n -1 | cut -d ' ' -f1 | sed "s/,//g" | sed '$!s/$/,/' | tr "\n" " " >> $FILEBASE
            printf "\n" >> $FILEBASE
        fi 

        B=$(( $B + 1 ))
        done 
    done
done



