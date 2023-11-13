#!/bin/bash

python getcycles.py

cat execution_time.csv | column -t -s, | less -S > result.out
