#!/bin/bash

#install except

CWD=($PWD)
echo $CWD
NUM_CPUS=8

if [ ! -d "gem5" ];then
	echo "gem5 directory does not exists"
	exit 1 
fi

gem5_path="$CWD/gem5_ssp"
gemOS_path="$CWD/gemOS_ssp_dirtybit"
script_path="$CWD/run_scripts_fig9/ssp_dirtybit"
disk_path="$CWD/disk_image"

echo $gem5_path
cd $gem5_path


#switching to ssp branch

git checkout -f ssp

git pull

#build gem5
echo "Building gem5 for ssp with $NUM_CPUS cpus"

scons build/X86/gem5.opt -j $NUM_CPUS

#Going to run Gapbs_pr, G500_sssp, Ycsb_mem

cd $script_path
cp ../run_nobypass.sh $gem5_path 
cp ../run.except $gem5_path

cp gapbs_dirtybit_run_10us.sh $gem5_path 
cp run_dirtybit_gapbs_10us.c $gem5_path 

cp gapbs_dirtybit_run_100us.sh $gem5_path 
cp run_dirtybit_gapbs_100us.c $gem5_path 

cp gapbs_dirtybit_run_1000us.sh $gem5_path 
cp run_dirtybit_gapbs_1000us.c $gem5_path 

cp sssp_dirtybit_run_10us.sh $gem5_path 
cp run_dirtybit_sssp_10us.c $gem5_path 

cp sssp_dirtybit_run_100us.sh $gem5_path 
cp run_dirtybit_sssp_100us.c $gem5_path 

cp sssp_dirtybit_run_1000us.sh $gem5_path 
cp run_dirtybit_sssp_1000us.c $gem5_path 

cp workloadb_dirtybit_run_10us.sh $gem5_path 
cp run_dirtybit_workloadb_10us.c $gem5_path 

cp workloadb_dirtybit_run_100us.sh $gem5_path 
cp run_dirtybit_workloadb_100us.c $gem5_path 

cp workloadb_dirtybit_run_1000us.sh $gem5_path 
cp run_dirtybit_workloadb_1000us.c $gem5_path 

######################
#ssp merger thread interval 10us

output_path="$CWD/output_fig9/ssp_dirtybit_10ms_10us"

echo "compiling gemOS for Gapbs_pr, G500_sssp, Ycsb_mem"
cd $gemOS_path

cp ./user/init_micro-benchmark_10us.c ./user/init_micro-benchmark.c

./compile.sh micro

cp gemOS_dirtybit.kernel gemOS_dirtybit_10us.kernel

cd $gem5_path

KERNEL_NAME_A="$gemOS_path/gemOS_dirtybit_10us.kernel"
export KERNEL_NAME_A

#gapbs_pr

OUTPUT_NAME_A="$output_path/gapbs"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_gapbs.img"
export DISK_NAME_A

gemos_out="$output_path/gapbs/gemos.out"
gemos_err="$output_path/gapbs/gemos.err"

gcc run_dirtybit_gapbs_10us.c -o run_dirtybit_gapbs_10us
./run_dirtybit_gapbs_10us $gemos_out $gemos_err

#G500_sssp

OUTPUT_NAME_A="$output_path/sssp"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_sssp.img"
export DISK_NAME_A

gemos_out="$output_path/sssp/gemos.out"
gemos_err="$output_path/sssp/gemos.err"

gcc run_dirtybit_sssp_10us.c -o run_dirtybit_sssp_10us
./run_dirtybit_sssp_10us $gemos_out $gemos_err

#Ycsb_mem

OUTPUT_NAME_A="$output_path/workloadb"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_workloadb.img"
export DISK_NAME_A

gemos_out="$output_path/workloadb/gemos.out"
gemos_err="$output_path/workloadb/gemos.err"

gcc run_dirtybit_workloadb_10us.c -o run_dirtybit_workloadb_10us
./run_dirtybit_workloadb_10us $gemos_out $gemos_err


######################
#ssp merger thread interval 100us

output_path="$CWD/output_fig9/ssp_dirtybit_10ms_100us"

echo "compiling gemOS"

cd $gemOS_path

cp ./user/init_micro-benchmark_100us.c ./user/init_micro-benchmark.c

./compile.sh micro

cp gemOS_dirtybit.kernel gemOS_dirtybit_100us.kernel

cd $gem5_path

KERNEL_NAME_A="$gemOS_path/gemOS_dirtybit_100us.kernel"
export KERNEL_NAME_A

#gapbs_pr

OUTPUT_NAME_A="$output_path/gapbs"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_gapbs.img"
export DISK_NAME_A

gemos_out="$output_path/gapbs/gemos.out"
gemos_err="$output_path/gapbs/gemos.err"

gcc run_dirtybit_gapbs_100us.c -o run_dirtybit_gapbs_100us
./run_dirtybit_gapbs_100us $gemos_out $gemos_err

#G500_sssp

OUTPUT_NAME_A="$output_path/sssp"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_sssp.img"
export DISK_NAME_A

gemos_out="$output_path/sssp/gemos.out"
gemos_err="$output_path/sssp/gemos.err"

gcc run_dirtybit_sssp_100us.c -o run_dirtybit_sssp_100us
./run_dirtybit_sssp_100us $gemos_out $gemos_err

#Ycsb_mem

OUTPUT_NAME_A="$output_path/workloadb"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_workloadb.img"
export DISK_NAME_A

gemos_out="$output_path/workloadb/gemos.out"
gemos_err="$output_path/workloadb/gemos.err"

gcc run_dirtybit_workloadb_100us.c -o run_dirtybit_workloadb_100us
./run_dirtybit_workloadb_100us $gemos_out $gemos_err


######################
#ssp merger thread interval 1ms

output_path="$CWD/output_fig9/ssp_dirtybit_10ms_1000us"

echo "compiling gemOS for Gapbs_pr, G500_sssp, Ycsb_mem"

cd $gemOS_path

cp ./user/init_micro-benchmark_1000us.c ./user/init_micro-benchmark.c

./compile.sh micro

cp gemOS_dirtybit.kernel gemOS_dirtybit_1000us.kernel

cd $gem5_path

KERNEL_NAME_A="$gemOS_path/gemOS_dirtybit_1000us.kernel"
export KERNEL_NAME_A

#gapbs_pr

OUTPUT_NAME_A="$output_path/gapbs"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_gapbs.img"
export DISK_NAME_A

gemos_out="$output_path/gapbs/gemos.out"
gemos_err="$output_path/gapbs/gemos.err"

gcc run_dirtybit_gapbs_1000us.c -o run_dirtybit_gapbs_1000us
./run_dirtybit_gapbs_1000us $gemos_out $gemos_err

#G500_sssp

OUTPUT_NAME_A="$output_path/sssp"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_sssp.img"
export DISK_NAME_A

gemos_out="$output_path/sssp/gemos.out"
gemos_err="$output_path/sssp/gemos.err"

gcc run_dirtybit_sssp_1000us.c -o run_dirtybit_sssp_1000us
./run_dirtybit_sssp_1000us $gemos_out $gemos_err

#Ycsb_mem

OUTPUT_NAME_A="$output_path/workloadb"
export OUTPUT_NAME_A
DISK_NAME_A="$disk_path/data_workloadb.img"
export DISK_NAME_A

gemos_out="$output_path/workloadb/gemos.out"
gemos_err="$output_path/workloadb/gemos.err"

gcc run_dirtybit_workloadb_1000us.c -o run_dirtybit_workloadb_1000us
./run_dirtybit_workloadb_1000us $gemos_out $gemos_err

