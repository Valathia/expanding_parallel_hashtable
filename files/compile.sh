gcc -c expht.c -o expht.o
ar rcu libexpht.a expht.o
gcc bench_.c libexpht.a -o bench_ -lpthread
