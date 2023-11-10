#!/bin/bash

#install except

CWD=($PWD)
echo $CWD
NUM_CPUS=8

if [ ! -d "gem5" ];then
	echo "gem5 directory does not exists"
	exit 1 
fi

gem5_path="$CWD/gem5"
gemOS_path="$CWD/gemOS_dirtybit"
script_path="$CWD/run_scripts_fig8/dirtybit"
output_path="$CWD/output_fig8/dirtybit"
disk_path="$CWD/disk_image"
echo $gem5_path
cd $gem5_path

#switching to Prosper branch

git checkout -f vanilla

git pull

#build gem5
echo "Building gem5 for Prosper with $NUM_CPUS cpus"

scons build/X86/gem5.opt -j $NUM_CPUS


#Going to run Gapbs_pr, G500_sssp, Ycsb_mem

cd $script_path
cp ../run_nobypass.sh $gem5_path 
cp ../run.except $gem5_path 
cp gapbs_run.sh $gem5_path 
cp run_gapbs.c $gem5_path 
cp sssp_run.sh $gem5_path 
cp run_sssp.c $gem5_path 
cp workloadb_run.sh $gem5_path 
cp run_workloadb.c $gem5_path 
cp sparse_run.sh $gem5_path 
cp run_sparse.c $gem5_path 
cp stream_run.sh $gem5_path 
cp run_stream.c $gem5_path 
cp random_run.sh $gem5_path 
cp run_random.c $gem5_path 


echo "compiling gemOS for Gapbs_pr, G500_sssp, Ycsb_mem"
cd $gemOS_path
./compile.sh micro
./compile.sh sparse
./compile.sh stream
./compile.sh random


cd $gem5_path

KERNEL_NAME="$gemOS_path/gemOS_dirtybit.kernel"
export KERNEL_NAME

#gapbs_pr

OUTPUT_NAME="$output_path/gapbs"
export OUTPUT_NAME
DISK_NAME="$disk_path/data_gapbs.img"
export DISK_NAME

gemos_out="$output_path/gapbs/gemos.out"
gemos_err="$output_path/gapbs/gemos.err"

gcc run_gapbs.c -o run_gapbs
./run_gapbs $gemos_out $gemos_err

#G500_sssp

OUTPUT_NAME="$output_path/sssp"
export OUTPUT_NAME
DISK_NAME="$disk_path/data_sssp.img"
export DISK_NAME

gemos_out="$output_path/sssp/gemos.out"
gemos_err="$output_path/sssp/gemos.err"

gcc run_sssp.c -o run_sssp
./run_sssp $gemos_out $gemos_err

#Ycsb_mem

OUTPUT_NAME="$output_path/workloadb"
export OUTPUT_NAME
DISK_NAME="$disk_path/data_workloadb.img"
export DISK_NAME

gemos_out="$output_path/workloadb/gemos.out"
gemos_err="$output_path/workloadb/gemos.err"

gcc run_workloadb.c -o run_workloadb
./run_workloadb $gemos_out $gemos_err

#Sparse 

KERNEL_NAME="$gemOS_path/gemOS_sparse.kernel"
export KERNEL_NAME

OUTPUT_NAME="$output_path/sparse"
export OUTPUT_NAME
DISK_NAME="$disk_path/gemos.img"
export DISK_NAME

gemos_out="$output_path/sparse/gemos.out"
gemos_err="$output_path/sparse/gemos.err"

gcc run_sparse.c -o run_sparse
./run_sparse $gemos_out $gemos_err

#Stream

KERNEL_NAME="$gemOS_path/gemOS_stream.kernel"
export KERNEL_NAME

OUTPUT_NAME="$output_path/stream"
export OUTPUT_NAME
DISK_NAME="$disk_path/gemos.img"
export DISK_NAME

gemos_out="$output_path/stream/gemos.out"
gemos_err="$output_path/stream/gemos.err"

gcc run_stream.c -o run_stream
./run_stream $gemos_out $gemos_err

#Random

KERNEL_NAME="$gemOS_path/gemOS_random.kernel"
export KERNEL_NAME

OUTPUT_NAME="$output_path/random"
export OUTPUT_NAME
DISK_NAME="$disk_path/gemos.img"
export DISK_NAME

gemos_out="$output_path/random/gemos.out"
gemos_err="$output_path/random/gemos.err"

gcc run_random.c -o run_random
./run_random $gemos_out $gemos_err


