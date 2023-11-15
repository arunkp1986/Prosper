Execute `format_output.sh` to invoke **getcopy\_linux_new.py** python script. It generate intermediate result.out files in **linux** folder.

these result.out files are generated for each benchmark application. For example **mcf\_s\_result.out** for mcf. 

similarly, **getcopy\_linux\_vanilla.py** python script generates intermediate result.out files in **linux_vanilla** folder corresponding to no dirty tracking.

`./format_output.sh`

`format_output.sh` script then calls **file\_read\_expected.py** and **file\_read\_result.py** to process intermediate files in **linux** and **linux_vanilla** folders, and produces expected.out and result.out

Compare result.out file with the expected result in expected.out file. E.g.: `diff result.out expected.out`

Note: **getcopy\_linux_new.py** and **getcopy\_linux\_vanilla.py** parses gem5 **stats.txt** files. The result.out file contains speedup numbers.

