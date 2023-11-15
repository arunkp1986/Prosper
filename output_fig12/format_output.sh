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

cat mcf_s_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/mcf_s_result.out
rm mcf_s_result_vanilla.csv
cat g500_sssp_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/g500_sssp_result.out
rm g500_sssp_result_vanilla.csv
cat gaps_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/gaps_result.out
rm gaps_result_vanilla.csv
cat leela_s_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/leela_s_result.out
rm leela_s_result_vanilla.csv
cat omnetpp_s_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/omnetpp_s_result.out
rm omnetpp_s_result_vanilla.csv
cat perlbench_s_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/perlbench_s_result.out
rm perlbench_s_result_vanilla.csv
cat stream_result_vanilla.csv | column -t -s, | less -S > ./linux_vanilla/stream_result.out
rm stream_result_vanilla.csv

python file_read_expected.py
cat expected.csv | column -t -s, | less -S > expected.out
rm expected.csv

python file_read_result.py
cat result.csv | column -t -s, | less -S > result.out
rm result.csv

