#!/bin/bash

#./run.sh LOCK N_TEST N_ELEM
# rename -n -e 's/.csv/_b.csv/' *.csv   
# /usr/include/jemalloc


DEBUG=0
DEBUG_FLAG=""
LOCK=""
LOCK_NAME="rw"

JEMALLOC=0
JEMALLOC_NAME="nojemalloc"


while getopts ":jld" opt; do
    case $opt in 
        j)
            JEMALLOC=1
            JEMALLOC_NAME="jemalloc"
            printf "Jemalloc is on\n"
            ;;
        l)
            printf "lock is Mutex\n"
            LOCK="-DMUTEX=1"
            LOCK_NAME="mutex"
            ;;
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

N_ELEM=${@:$OPTIND+1:1}
VERSION=${@:$OPTIND+2:1}


gcc -c expht_${VERSION}.c ${LOCK} -o expht.o
ar rcu libexpht.a expht.o

case $VERSION in
    a)
        printf "picked version a\n"
        gcc bench_.c libexpht.a -O3 ${DEBUG_FLAG} ${LOCK} -lpthread -o bench
    ;;
    b)
        printf "picked version b\n"
        gcc bench_.c libexpht.a -O3 ${DEBUG_FLAG} ${LOCK} -lpthread -o bench
    ;;
    c)
        printf "picked version c\n"
        gcc bench_.c libexpht.a -O3 ${DEBUG_FLAG} ${LOCK} -lpthread -o bench   
    ;;
    d)
        printf "picked version d\n"
        gcc bench_.c libexpht.a -O3 ${DEBUG_FLAG} ${LOCK} -DARRAY=1 -lpthread -o bench   
    ;;
    e)
        printf "picked version e\n"
        gcc bench_.c libexpht.a -O3 ${DEBUG_FLAG} ${LOCK} -DARRAY=1 -lpthread -o bench   
    ;;
    f)
        printf "picked version f\n"
        gcc bench_.c libexpht.a -O3 ${DEBUG_FLAG} ${LOCK} -DARRAY=1 -lpthread -o bench   
    ;;
    *)
        printf "I need to know which version of the struct you wanna use mate \n"
        exit 1
    ;;
esac


printf "Testing: ${LOCK_NAME} lock w/ struct ${VERSION} ${JEMALLOC_NAME} \n"

N_TH=1
MAX_TH=33

while [ "$N_TH" -lt "$MAX_TH" ]
do

    if [ "$DEBUG" -eq 1 ]
    then 
        printf "Printing to debug folder \n"
        FILE="../tests/debug/${LOCK_NAME}/${VERSION}/${N_TH}_${N_ELEM}_${LOCK_NAME}_${VERSION}_${JEMALLOC_NAME}.csv"
    else 
        FILE="../tests/${LOCK_NAME}/${VERSION}/${N_TH}_${N_ELEM}_${LOCK_NAME}_${VERSION}_${JEMALLOC_NAME}.csv"
    fi
    
    printf "inserts real time, inserts process time, hashtable size, elements recorded, elements total, elements actual, largest element size, smallest element size, average element size, largest bucket size, smallest bucket size, average bucket size, deletes real time, deletes process time, hits real time, hits process time, misses real time, misses process time, mixed real time, mixed process time\n" > $FILE

    B=0;

    while [ "$B" -lt "$N_TEST" ]
    do
    #printf "Test %d: #Threads: %d - All inserts \n" $A $N_TH # | tee log.txt    

    #pegar no output e tirar os tempos
    printf "Test %d: #Threads: %d Lock: %s Mode: %s  " $B $N_TH $LOCK_NAME $JEMALLOC_NAME # | tee log.txt   
    
    if [ "$JEMALLOC" -eq 1 ]
    then
        printf "Testing with jemalloc preload \n"
        #JEMALLOC 
        #overwrite file then add the others to file
        printf "testing all inserts..."
        LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` ./bench $N_TH $N_ELEM 100 0 0 0  > ../tests/times.txt
        printf "testing all deletes..."
        LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` ./bench $N_TH $N_ELEM 0 100 0 0  >> ../tests/times.txt
        printf "testing all hits..."
        LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` ./bench $N_TH $N_ELEM 0 0 100 0  >> ../tests/times.txt
        printf "testing all misses..."
        LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` ./bench $N_TH $N_ELEM 0 0 0 100  >> ../tests/times.txt
        printf "testing mixed...\n"
        LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision` ./bench $N_TH $N_ELEM 50 20 20 10  >> ../tests/times.txt

    else
        #overwrite file then add the others to file
        printf "testing all inserts..."
        ./bench $N_TH $N_ELEM 100 0 0 0  > ../tests/times.txt
        printf "testing all deletes..."
        ./bench $N_TH $N_ELEM 0 100 0 0  >> ../tests/times.txt
        printf "testing all hits..."
        ./bench $N_TH $N_ELEM 0 0 100 0  >> ../tests/times.txt
        printf "testing all misses..."
        ./bench $N_TH $N_ELEM 0 0 0 100  >> ../tests/times.txt
        printf "testing mixed...\n"
        ./bench $N_TH $N_ELEM 50 20 20 10  >> ../tests/times.txt
    fi

    #throw away 1st test
    if [ 0 -lt "$B" ]; then
        # get only numbers -> take empty lines out -> -> replace \n with a space -> output to csv file
        sed  "s/[^0-9].[^0-9]*//" ../tests/times.txt  | sed '/^\s*$/d' | sed '$!s/$/,/' | tr "\n" " " >> $FILE
        printf "\n" >> $FILE
    fi 

    B=$(( $B + 1 ))
    done 

    N_TH=$(( $N_TH + 1 ))
done