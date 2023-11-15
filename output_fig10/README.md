Execute `format_output.sh` to invoke **getcycles.py** and **getcopy\_time.py** python scripts and generate `result_*.out` files.

`./format_output.sh`

Compare `result_*.out` file with the expected results in the corresponding `expected_*.out` file. 

E.g.: `diff expected_checkpoint_size.out result_checkpoint_size.out`

Note: **getcycles.py** parses gem5 **stats.txt** files in output folders and generates a **.csv** file containing checkpoint size. **getcopy\_time.py** parses gem5 **stats.txt** files in output folders and generates a **.csv** file containing checkpoint time with Prosper normalized to checkpoint time with page granularity dirty tracking. `format_output.sh` uses this **.csv** file to produce formatted `result_*.out`.
