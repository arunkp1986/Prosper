Execute `format_output.sh` to invoke **getcycles.py** python script and generate `result_*.out` files.

`./format_output.sh`

Compare `result_*.out` file with the expected results in the corresponding `expected_*.out` file. E.g.: `diff result_1ms.out expected_1ms.out`

Note: **getcycles.py** parses gem5 **stats.txt** files in output folders and generates a **.csv** file containing execution time normalized to no persistence (i.e., vanilla). `format_output.sh` uses this **.csv** file to produce formatted `result_*.out`.
