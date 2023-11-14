#!/bin/bash

python getcycles.py

grep 1000us execution_time.csv > result_1ms.out
grep 100us execution_time.csv > result_100us.out
grep 10us execution_time.csv > result_10us.out

sed -i -e 's/ssp_dirtybit_10ms_1000us/ssp_dirtybit/g' result_1ms.out
sed -i -e 's/ssp_dirtybit_10ms_100us/ssp_dirtybit/g' result_100us.out
sed -i -e 's/ssp_dirtybit_10ms_10us/ssp_dirtybit/g' result_10us.out

sed -i -e 's/ssp_prosper_10ms_1000us/ssp_prosper/g' result_1ms.out
sed -i -e 's/ssp_prosper_10ms_100us/ssp_prosper/g' result_100us.out
sed -i -e 's/ssp_prosper_10ms_10us/ssp_prosper/g' result_10us.out

sed -i -e 's/ssp_stack_heap_10ms_1000us/ssp/g' result_1ms.out
sed -i -e 's/ssp_stack_heap_10ms_100us/ssp/g' result_100us.out
sed -i -e 's/ssp_stack_heap_10ms_10us/ssp/g' result_10us.out

sed -i '1 i type,gapbs,sssp,workloadb' result_1ms.out
cat result_1ms.out | column -t -s, | less -S > result.out
mv result.out result_1ms.out
sed -i '1 i type,gapbs,sssp,workloadb' result_100us.out
cat result_100us.out | column -t -s, | less -S > result.out
mv result.out result_100us.out
sed -i '1 i type,gapbs,sssp,workloadb' result_10us.out
cat result_10us.out | column -t -s, | less -S > result.out
mv result.out result_10us.out


