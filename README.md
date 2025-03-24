to run:
./run.sh  LOCK N_TEST N_ELEM

LOCK: 

0 - Read/Write Locks

1 - Mutex

N_TEST: Nº of tests per Thread Nº. 
Ex: ./run.sh 1 10 1000 
will run Mutex Locks, and will do 10 tests per each Nº of Threads in use (1,2,3,...,64). 
this equates to: 10 all inserts, 10 all deletes, 10 all hits , 10 all misses and 10 mixed ratio tests. per each Thread count. and work with 1000 elements per test.