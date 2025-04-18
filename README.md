##Run

to run:
./run.sh N_TEST N_ELEM STRUCT

opt flags: -l -j

-l : activates mutex lock (otherwise read/write lock)

-j : substitutes malloc with jemalloc in runtime using LD-PRELOAD

-d : activates debug mode for bench

N_TEST: Nº of tests per Thread Nº. 

Ex: ./run.sh -l 10 1000 a

will run Mutex Locks, and will do 10 tests per each Nº of Threads in use (1,2,3,...,32), using struct a.

this equates to: 10 all inserts, 10 all deletes, 10 all hits , 10 all misses and 10 mixed ratio tests. per each Thread count. and work with 1000 elements per test.

###Structs:

- a: Default Expandable Hashtable w/ Nodes

- c: Expandable Hashtable w/ Nodes & Delayed Updates

- d: Expandable Hashtable w/ Ordered Arrays & Delayed Updates

- e: Expandable Hashtable w/ Unordered Arrays & Delayed Updates

- f: Expandable Hashtable w/ Nodes & Delayed Updates & Semi Co-op expansion

- g: Expandable Hashtable w/ Unordered Arrays & Delayed Updates & Semi Co-op expansion