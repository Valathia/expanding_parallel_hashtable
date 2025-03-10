./compile_debug.sh
printf "1 Thread Test\n"
./bench_debug 1 100000 50 20 20 10 >> ../tests/test1_100000_debug.txt

printf "2 Thread Test\n"
./bench_debug 2 100000 50 20 20 10 >> ../tests/test2_100000_debug.txt

printf "4 Thread test \n"
./bench_debug 4 100000 50 20 20 10 >> ../tests/test4_100000_debug.txt

printf "8 Thread test \n"
./bench_debug 8 100000 50 20 20 10 >> ../tests/test8_100000_debug.txt

printf "12 Thread test \n"
./bench_debug 12 100000 50 20 20 10 >> ../tests/test12_100000_debug.txt