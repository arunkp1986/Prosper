Execute `format_output.sh` to invoke **getcopy\_interval.py** python script and generate result.out files.

`./format_output.sh`

Compare result.out file with the expected result in `expected.out` file. E.g.: `diff result.out expected.out`

Note: **getcopy\_interval.py** parses gem5 **stats.txt** files in output folders and generates a **.csv** file containing checkpoint size based on checkpoint interval. `format_output.sh` uses this **.csv** file to produce formatted result.out.
