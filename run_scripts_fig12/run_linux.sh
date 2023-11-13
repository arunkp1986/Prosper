#!/bin/bash

outdir=$1
kernel=$2
disk=$3
bench=$4

echo $outdir
echo $kernel
echo $disk
echo $bench

./build/X86/gem5.opt --outdir=$outdir configs/spec_config/run_spec.py $kernel $disk "timing" "$bench" "test"

