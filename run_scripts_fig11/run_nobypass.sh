#!/bin/bash

outdir=$1
kernel=$2
disk=$3

if [[ $# -ne 3 ]];then
    echo "pass output dir,kernel,disk"
    exit 2
fi

echo $outdir
echo $kernel
echo $disk


./build/X86/gem5.opt --outdir=$outdir configs/example/fs.py --mem-size=3GB --nvm-size=2GB --caches --l3cache --cpu-type TimingSimpleCPU --hybrid-channel True --mem-type=DDR4_2400_16x4 --nvm-type=NVM_2666_1x64 --kernel=$kernel --disk-image=$disk
