#!/bin/bash

#install except

CWD=($PWD)
echo $CWD
NUM_CPUS=8

if [ ! -d "gem5" ];then
	echo "gem5 directory does not exists"
	exit 1 
fi

gem5_path="$CWD/gem5_prosper_linux"
linux_path="$CWD/disk_image/linux_kernels"
script_path="$CWD/run_scripts_fig13"
output_path="$CWD/output_fig13"
disk_path="$CWD/disk_image"
echo $gem5_path
cd $gem5_path


#switching to Prosper branch

git checkout -f linux_prosper

git pull

#build gem5
echo "Building gem5 for Prosper with $NUM_CPUS cpus"

scons build/X86/gem5.opt -j $NUM_CPUS

#Going to run Gapbs_pr, G500_sssp, Ycsb_mem

cd $script_path

cp run_linux.sh $gem5_path 

cd $gem5_path

KERNEL_NAME="$linux_path/vmlinux-v5.2.3"

DISK_NAME="$disk_path/linux_disk"


#G500_sssp and mcf HWM

for hwm in 4HWM 8HWM 16HWM 24HWM
do 
	echo $hwm
	
	if [ $hwm == "4HWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_4hwm.hh" "$gem5_path/src/cpu/simple/timing.hh" 
	        scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	elif [ $hwm == "8HWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_8hwm.hh" "$gem5_path/src/cpu/simple/timing.hh"
		scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	elif [ $hwm == "16HWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_16hwm.hh" "$gem5_path/src/cpu/simple/timing.hh"
		scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	elif [ $hwm == "24HWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_24hwm.hh" "$gem5_path/src/cpu/simple/timing.hh"
		scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$hwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	fi
	
done

#G500_sssp and mcf LWM

for lwm in 4LWM 8LWM 16LWM 20LWM
do 
	echo $lwm
	
	if [ $lwm == "4LWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_4lwm.hh" "$gem5_path/src/cpu/simple/timing.hh" 
	        scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	elif [ $lwm == "8LWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_8lwm.hh" "$gem5_path/src/cpu/simple/timing.hh"
		scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	elif [ $lwm == "16LWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_16lwm.hh" "$gem5_path/src/cpu/simple/timing.hh"
		scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	elif [ $lwm == "20LWM" ];then
		cp "$gem5_path/src/cpu/simple/timing_20lwm.hh" "$gem5_path/src/cpu/simple/timing.hh"
		scons build/X86/gem5.opt -j $NUM_CPUS
		OUTPUT_NAME="$output_path/linux/g500_sssp/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "g500_8_1"
		OUTPUT_NAME="$output_path/linux/mcf_s/$lwm"
		./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME "mcf_8_1"
	fi
	
done


