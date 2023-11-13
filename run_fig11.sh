#!/bin/bash

#install except

CWD=($PWD)
echo $CWD
NUM_CPUS=8

if [ ! -d "gem5" ];then
	echo "gem5 directory does not exists"
	exit 1 
fi

gem5_path="$CWD/gem5_prosper"
gemOS_path="$CWD/gemOS_prosper"
script_path="$CWD/run_scripts_fig11"
disk_path="$CWD/disk_image"

echo $gem5_path
cd $gem5_path

#switching to Prosper branch

git checkout -f merged_prosper_v3

git pull

#build gem5

echo "Building gem5 for Prosper with $NUM_CPUS cpus"

scons build/X86/gem5.opt -j $NUM_CPUS


echo "Copying run scripts to gem5 folder"

cd $script_path
cp run_nobypass.sh $gem5_path 
cp run.except $gem5_path 

for g in 1ms 5ms 10ms
do
	cp quick_run_$g.sh $gem5_path 
	cp run_quick_$g.c $gem5_path 
	cp recursive4_run_$g.sh $gem5_path
	cp run_recursive4_$g.c $gem5_path 
	cp recursive8_run_$g.sh $gem5_path
	cp run_recursive8_$g.c $gem5_path 
	cp recursive16_run_$g.sh $gem5_path
	cp run_recursive16_$g.c $gem5_path 
done

echo "gemOS compilation"

cd $gemOS_path

for g in 1ms 5ms 10ms
do
   ./compile_fig11.sh quick $g
   ./compile_fig11.sh recursive4 $g
   ./compile_fig11.sh recursive8 $g
   ./compile_fig11.sh recursive16 $g
done


echo "Benchmark run"

cd $gem5_path

for g in 1ms 5ms 10ms
do
	#Quick
	
	output_path="$CWD/output_fig11/quick/byte/interval/$g"
	
	KERNEL_NAME_C="$gemOS_path/gemOS_quick_$g.kernel"
	export KERNEL_NAME_C
	
	OUTPUT_NAME_C="$output_path"
	export OUTPUT_NAME_C

	DISK_NAME_C="$disk_path/gemos.img"
	export DISK_NAME_C

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_quick_$g.c -o run_quick_$g
	./run_quick_$g $gemos_out $gemos_err

	#Rec4

	output_path="$CWD/output_fig11/recursive_4/$g"

	KERNEL_NAME_C="$gemOS_path/gemOS_recursive4_$g.kernel"
	export KERNEL_NAME_C

	OUTPUT_NAME_C="$output_path"
	export OUTPUT_NAME_C

	DISK_NAME_C="$disk_path/gemos.img"
	export DISK_NAME_C

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_recursive4_$g.c -o run_recursive4_$g
	./run_recursive4_$g $gemos_out $gemos_err

	#Rec8

	output_path="$CWD/output_fig11/recursive_8/$g"

	KERNEL_NAME_C="$gemOS_path/gemOS_recursive8_$g.kernel"
	export KERNEL_NAME_C

	OUTPUT_NAME_C="$output_path"
	export OUTPUT_NAME_C

	DISK_NAME_C="$disk_path/gemos.img"
	export DISK_NAME_C

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_recursive8_$g.c -o run_recursive8_$g
	./run_recursive8_$g $gemos_out $gemos_err

	#Rec16

	output_path="$CWD/output_fig11/recursive_16/$g"

	KERNEL_NAME_C="$gemOS_path/gemOS_recursive16_$g.kernel"
	export KERNEL_NAME_C

	OUTPUT_NAME_C="$output_path"
	export OUTPUT_NAME_C

	DISK_NAME_C="$disk_path/gemos.img"
	export DISK_NAME_C

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_recursive16_$g.c -o run_recursive16_$g
	./run_recursive16_$g $gemos_out $gemos_err

done
