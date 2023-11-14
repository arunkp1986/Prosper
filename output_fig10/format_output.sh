#!/bin/bash

python getcopy.py

cat result_checkpoint_size.csv | column -t -s, | less -S > result_checkpoint_size.out

rm result_checkpoint_size.csv

python getcopy_time.py

cat result_checkpoint_time.csv | column -t -s, | less -S > result_checkpoint_time.out

rm result_checkpoint_time.csv
