#!/bin/bash

begin=$1
for i in `seq 0 15`;
do
	this=$(($begin + $i))
	kill -9 $this 
done
