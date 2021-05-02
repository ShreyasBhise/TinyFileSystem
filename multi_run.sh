#!/bin/sh
a=0

while [ "$a" -lt 200 ]
do
    ./auto_run.sh
    a=`expr $a + 1`
done