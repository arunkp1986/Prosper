#!/bin/bash

python getcopy_linux_new.py

cat mcf_s_result.csv | column -t -s, | less -S > ./linux/mcf_s_result.out
rm mcf_s_result.csv
cat g500_sssp_result.csv | column -t -s, | less -S > ./linux/g500_sssp_result.out
rm g500_sssp_result.csv
cat gaps_result.csv | column -t -s, | less -S > ./linux/gaps_result.out
rm gaps_result.csv
cat leela_s_result.csv | column -t -s, | less -S > ./linux/leela_s_result.out
rm leela_s_result.csv
cat omnetpp_s_result.csv | column -t -s, | less -S > ./linux/omnetpp_s_result.out
rm omnetpp_s_result.csv
cat perlbench_s_result.csv | column -t -s, | less -S > ./linux/perlbench_s_result.out
rm perlbench_s_result.csv
cat stream_result.csv | column -t -s, | less -S > ./linux/stream_result.out
rm stream_result.csv

python getcopy_linux_vanilla.py

cat mcf_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/mcf_s_result.out
rm mcf_result_vanilla.csv
cat g500_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/g500_sssp_result.out
rm g500_result_vanilla.csv
cat gap_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/gaps_result.out
rm gap_result_vanilla.csv
cat leela_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/leela_s_result.out
rm leela_result_vanilla.csv
cat omnetpp_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/omnetpp_s_result.out
rm omnetpp_result_vanilla.csv
cat perlbench_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/perlbench_s_result.out
rm perlbench_result_vanilla.csv
cat stream_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/stream_result.out
rm stream_result_vanilla.csv

