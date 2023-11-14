#!/bin/bash

python getcycles.py

cat execution_time.csv | column -t -s, | less -S > result.out

rm execution_time.csv
