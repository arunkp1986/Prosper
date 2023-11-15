Execute `format_output.sh` to invoke **getcopy\_linux\_hwm.py** python script and generate result.out files for HWM results corresponding to mcf and g500\_sssp.

`format_output.sh` also invokes **getcopy\_linux\_lwm.py** python script and generate result.out files for LWM.

`./format_output.sh`

Compare HWM and LWM result.out files with the expected result files of corresponding benchmarks. 

E.g.: `diff g500_sssp_hwm_expected.out g500_sssp_hwm_result.out` and `diff mcf_lwm_expected.out mcf_lwm_result.out`

Note: **getcopy\_linux\_hwm.py** and **getcopy\_linux\_lwm.py** parses gem5 **stats.txt** files in output folders and generates **.csv** file containing 
count of bitmap load and store operations. `format_output.sh` uses this **.csv** file to produce formatted result.out files.
