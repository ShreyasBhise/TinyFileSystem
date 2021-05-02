#!/bin/sh
netid=syb29
rm DISKFILE > /dev/null
./tfs -s /tmp/$netid/mountdir
#echo "Mounted /tmp/syb29/mountdir"
cd benchmark
#echo "Running simple_test.c"
./simple_test >> ../simplelog
cd ..
fusermount -u /tmp/$netid/mountdir
#echo "Unmounted /tmp/syb29/mountdir"
rm DISKFILE > /dev/null
./tfs -s /tmp/$netid/mountdir
#echo "Mounted /tmp/syb29/mountdir"
cd benchmark
#echo "Running test_cases.c"
./test_case >> ../testlog
cd ..
fusermount -u /tmp/$netid/mountdir