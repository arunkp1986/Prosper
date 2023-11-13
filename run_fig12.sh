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
script_path="$CWD/run_scripts_fig12"
output_path="$CWD/output_fig12"
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

#gapbs_pr

for bench in gap_0_1 gap_8_1 gap_64_1 gap_128_1
do 
	echo $bench
	
	if [ $bench == "gap_0_1" ];then
		OUTPUT_NAME="$output_path/linux/gaps/0bytes"
	elif [ $bench == "gap_8_1" ];then
		OUTPUT_NAME="$output_path/linux/gaps/8bytes"
	elif [ $bench == "gap_64_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/gaps/64bytes"
	elif [ $bench == "gap_128_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/gaps/128bytes"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done

#G500_sssp

for bench in g500_0_1 g500_8_1 g500_64_1 g500_128_1
do 
	echo $bench
	
	if [ $bench == "g500_0_1" ];then
		OUTPUT_NAME="$output_path/linux/g500_sssp/0bytes"
	elif [ $bench == "g500_8_1" ];then
		OUTPUT_NAME="$output_path/linux/g500_sssp/8bytes"
	elif [ $bench == "g500_64_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/g500_sssp/64bytes"
	elif [ $bench == "g500_128_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/g500_sssp/128bytes"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done



#stream

for bench in stream_0_1 stream_8_1 stream_64_1 stream_128_1
do 
	echo $bench
	
	if [ $bench == "stream_0_1" ];then
		OUTPUT_NAME="$output_path/linux/stream/0bytes"
	elif [ $bench == "stream_8_1" ];then
		OUTPUT_NAME="$output_path/linux/stream/8bytes"
	elif [ $bench == "stream_64_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/stream/64bytes"
	elif [ $bench == "stream_128_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/stream/128bytes"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done

#mcf

for bench in mcf_0_1 mcf_8_1 mcf_64_1 mcf_128_1
do 
	echo $bench
	
	if [ $bench == "mcf_0_1" ];then
		OUTPUT_NAME="$output_path/linux/mcf_s/0bytes"
	elif [ $bench == "mcf_8_1" ];then
		OUTPUT_NAME="$output_path/linux/mcf_s/8bytes"
	elif [ $bench == "mcf_64_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/mcf_s/64bytes"
	elif [ $bench == "mcf_128_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/mcf_s/128bytes"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done


#omnetpp

for bench in omnetpp_0_1 omnetpp_8_1 omnetpp_64_1 omnetpp_128_1
do 
	echo $bench
	
	if [ $bench == "omnetpp_0_1" ];then
		OUTPUT_NAME="$output_path/linux/omnetpp_s/0bytes"
	elif [ $bench == "omnetpp_8_1" ];then
		OUTPUT_NAME="$output_path/linux/omnetpp_s/8bytes"
	elif [ $bench == "omnetpp_64_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/omnetpp_s/64bytes"
	elif [ $bench == "omnetpp_128_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/omnetpp_s/128bytes"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done

#Perlbench

for bench in perlbench_0_1 perlbench_8_1 perlbench_64_1 perlbench_128_1
do 
	echo $bench
	
	if [ $bench == "perlbench_0_1" ];then
		OUTPUT_NAME="$output_path/linux/perlbench_s/0bytes"
	elif [ $bench == "perlbench_8_1" ];then
		OUTPUT_NAME="$output_path/linux/perlbench_s/8bytes"
	elif [ $bench == "perlbench_64_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/perlbench_s/64bytes"
	elif [ $bench == "perlbench_128_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/perlbench_s/128bytes"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done

#leela

for bench in leela_0_1 leela_8_1 leela_64_1 leela_128_1
do 
	echo $bench
	
	if [ $bench == "leela_0_1" ];then
		OUTPUT_NAME="$output_path/linux/leela_s/0bytes"
	elif [ $bench == "leela_8_1" ];then
		OUTPUT_NAME="$output_path/linux/leela_s/8bytes"
	elif [ $bench == "leela_64_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/leela_s/64bytes"
	elif [ $bench == "leela_128_1" ];then
		OUTPUT_NAME="$ouitput_path/linux/leela_s/128bytes"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done

#vanilla

gem5_path="$CWD/gem5"
linux_path="$CWD/disk_image/linux_kernels"
script_path="$CWD/run_scripts_fig12"
output_path="$CWD/output_fig12"
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

cp run_linux.sh $gem5_path 

cd $gem5_path

KERNEL_NAME="$linux_path/vmlinux_vanilla"

DISK_NAME="$disk_path/linux_disk"

for bench in mcf_0_0 omnetpp_0_0 perlbench_0_0 leela_0_0 g500_0_0 gap_0_0 stream_0_0
do 
	echo $bench
	
	if [ $bench == "mcf_0_0" ];then
		OUTPUT_NAME="$output_path/linux_vanilla/mcf"
	elif [ $bench == "omnetpp_0_0" ];then
		OUTPUT_NAME="$output_path/linux_vanilla/omnetpp"
	elif [ $bench == "perlbench_0_0" ];then
		OUTPUT_NAME="$output_path/linux_vanilla/perlbench"
	elif [ $bench == "leela_0_0" ];then
		OUTPUT_NAME="$output_path/linux_vanilla/leela"
        elif [ $bench == "g500_0_0" ];then
		OUTPUT_NAME="$output_path/linux_vanilla/g500"
        elif [ $bench == "gap_0_0" ];then
		OUTPUT_NAME="$output_path/linux_vanilla/gap"
        elif [ $bench == "stream_0_0" ];then
		OUTPUT_NAME="$output_path/linux_vanilla/stream"
	fi
	
	./run_linux.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME $bench
done

