##Run

to run:
./energy_tests.sh N_TESTS VERSION N_ELEM THRESH

opt flags: -d

-d : activates debug mode for bench

N_TEST: Nº of tests per Thread Nº per Test Type
VERSION: "list","array"
N_ELEM: How many operations per Test 
THRESH: LoadFactor+1 (usually 4 or 6)

THREADS=(1 2 4 8 12 16 20)
Tests: 100% searches, 100% inserts, 100% deletes, 10% I/R | 80% Seaches, 20% I/R | 60% Searches, 30% I/R | 60% Seaches 

Ex: ./energy_tests.sh 10 list 8388608 6

will run list struct, 10 tests per each Nº of Threads in use (1 2 4 8 12 16 20) with a Threshold of 6 and LoadFactor of 5, per Test type.

this equates to: 10 tests per each Thread count. and work with 8388608 elements/operations per test, which produces 6 csv files, one per Test type. (It's actually 18 files, each full test produces 3 csv's)

###Structs:

- array: Concurrent Expandable Hashtable with Arrays at Bucket Level, Closed Addressing, Dellayed Header Updates, Co-Op Expansion, Locks at bucket level

- list: Concurrent Expandable Hashtable with Nodes at Bucket Level, Closed Addressing, Delayed Header Updates, Co-Op Expansion, Locks at bucket level
- 
