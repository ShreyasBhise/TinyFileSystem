#!/bin/sh
make clean > /dev/null
make  > /dev/null
./tfs -s /tmp/syb29/mountdir
#echo "Mounted /tmp/syb29/mountdir"
cd benchmark
make clean  > /dev/null
make  > /dev/null
#echo "Running simple_test.c"
./simple_test >> ../simplelog
make clean  > /dev/null
cd ..
fusermount -u /tmp/syb29/mountdir
#echo "Unmounted /tmp/syb29/mountdir"
make clean  > /dev/null
make > /dev/null
./tfs -s /tmp/syb29/mountdir
#echo "Mounted /tmp/syb29/mountdir"
cd benchmark
make clean  > /dev/null
make  > /dev/null
#echo "Running test_cases.c"
./test_case >> ../testlog
make clean  > /dev/null
cd ..
fusermount -u /tmp/syb29/mountdir