#!/bin/bash

python getcopy_linux_hwm.py

cat mcf_s_hwm_result.csv | column -t -s, | less -S > mcf_hwm_result.out

rm mcf_s_hwm_result.csv

cat g500_sssp_hwm_result.csv | column -t -s, | less -S > g500_sssp_hwm_result.out

rm g500_sssp_hwm_result.csv

python getcopy_linux_lwm.py

cat mcf_s_lwm_result.csv | column -t -s, | less -S > mcf_lwm_result.out

rm mcf_s_lwm_result.csv

cat g500_sssp_lwm_result.csv | column -t -s, | less -S > g500_sssp_lwm_result.out

rm g500_sssp_lwm_result.csv
