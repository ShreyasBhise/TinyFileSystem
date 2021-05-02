#!/bin/sh
rm simplelog
rm testlog
a=0

while [ "$a" -lt 1000 ]
do
    ./auto_run.sh
    a=`expr $a + 1`
done

python average.py