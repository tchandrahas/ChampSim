#!/bin/bash
while read LINE
do
    ./run_champsim.sh bimodal-no-no-no-no-drrip-1core 200 1000 $LINE &
done < scripts/dpc3_max_simpoint.txt