#!/bin/bash

python getcopy_interval.py

cat copy_size_interval.csv | column -t -s, | less -S > result.out

rm copy_size_interval.csv
