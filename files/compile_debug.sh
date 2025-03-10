gcc -c parallel.c -o expht.o
ar rcu libexpht.a expht.o
gcc bench_.c libexpht.a -g -ggdb -Og -DDEBUG=1 -lpthread -o bench_debug
