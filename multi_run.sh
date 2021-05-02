#!/bin/sh

a=0
numtest=1000000
while [ "$a" -lt "$numtest" ]
do
    ./auto_run.sh
    a=`expr $a + 1`
done

python average.py $a
rm simplelog
rm testlog