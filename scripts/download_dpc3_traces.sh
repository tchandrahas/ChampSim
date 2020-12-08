#!/bin/bash

mkdir -p $PWD/../benchmark_traces
while read LINE
do
    wget -P $PWD/../benchmark_traces -c http://hpca23.cse.tamu.edu/champsim-traces/speccpu/$LINE
done < dpc3_max_simpoint.txt
