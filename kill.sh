#!/bin/bash

# tired of wasting time killing process one at a time????????

# boy! do i have the solution for you!

# this script:
# > looks for oss, kills it
# > looks for user (or child), kills the next 18 processes (cause why not)
# > remove all of your shared memory segments

# by Austin Hester
# ps someone run this logged in as o1-kaur pls

oss=`ps -a | grep oss | awk 'NR==1{ print $1 }'`
kill -9 $oss 2> /dev/null || true

begin=`ps | grep -e user -e child | awk 'NR==1{ print $1 }'`
for i in `seq 0 16`;
do
	this=$(($begin + $i))
	kill -9 $this || true
done
`ipcrm -all 2> /dev/null` || true
