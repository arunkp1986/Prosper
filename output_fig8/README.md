Execute `format_output.sh` to invoke **getcycles.py** python script and generate result.out files.

`./format_output.sh`

Compare result.out file with the expected result in `expected.out` file. E.g.: `diff result.out expected.out`

Note: **getcycles.py** parses gem5 **stats.txt** files in output folders and generates a **.csv** file containing execution time normalized to no persistence (i.e., vanilla). `format_output.sh` uses this **.csv** file to produce formatted result.out.
