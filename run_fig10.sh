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
script_path="$CWD/run_scripts_fig10"
disk_path="$CWD/disk_image"

echo $gem5_path
cd $gem5_path


#switching to Prosper branch

git checkout -f merged_prosper_v3

git pull

#build gem5
echo "Building gem5 for Prosper with $NUM_CPUS cpus"

scons build/X86/gem5.opt -j $NUM_CPUS

#Going to run Gapbs_pr, G500_sssp, Ycsb_mem

cd $script_path
cp run_nobypass.sh $gem5_path 
cp run.except $gem5_path 
for g in 8 16 32 64 128
do
	cp random_run_$g.sh $gem5_path 
	cp run_random_$g.c $gem5_path 
	cp sparse_run_$g.sh $gem5_path
	cp run_sparse_$g.c $gem5_path 
	cp stream_run_$g.sh $gem5_path
	cp run_stream_$g.c $gem5_path 
	cp normal_run_$g.sh $gem5_path
	cp run_normal_$g.c $gem5_path 
	cp poisson_run_$g.sh $gem5_path 
	cp run_poisson_$g.c $gem5_path 
	cp quicksort_run_$g.sh $gem5_path 
	cp run_quicksort_$g.c $gem5_path 
done

echo "compiling gemOS"

cd $gemOS_path

for g in 8 16 32 64 128
do
   ./compile_fig10.sh sparse $g
   ./compile_fig10.sh stream $g
   ./compile_fig10.sh random $g
   ./compile_fig10.sh quick $g
   ./compile_fig10.sh normal $g
   ./compile_fig10.sh poisson $g
done


cd $gem5_path


for g in 8 16 32 64 128
do
	#Sparse 
	
	output_path="$CWD/output_fig10/sparse/byte/gran/$g"
	
	KERNEL_NAME_B="$gemOS_path/gemOS_sparse_$g.kernel"
	export KERNEL_NAME_B
	
	OUTPUT_NAME_B="$output_path"
	export OUTPUT_NAME_B

	DISK_NAME_B="$disk_path/gemos.img"
	export DISK_NAME_B

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_sparse_$g.c -o run_sparse_$g
	./run_sparse_$g $gemos_out $gemos_err

	#Stream

	output_path="$CWD/output_fig10/stream/byte/gran/$g"

	KERNEL_NAME_B="$gemOS_path/gemOS_stream_$g.kernel"
	export KERNEL_NAME_B

	OUTPUT_NAME_B="$output_path"
	export OUTPUT_NAME_B

	DISK_NAME_B="$disk_path/gemos.img"
	export DISK_NAME_B

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_stream_$g.c -o run_stream_$g
	./run_stream_$g $gemos_out $gemos_err

	#Random

	output_path="$CWD/output_fig10/random/byte/gran/$g"

	KERNEL_NAME_B="$gemOS_path/gemOS_random_$g.kernel"
	export KERNEL_NAME_B

	OUTPUT_NAME_B="$output_path"
	export OUTPUT_NAME_B

	DISK_NAME_B="$disk_path/gemos.img"
	export DISK_NAME_B

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_random_$g.c -o run_random_$g
	./run_random_$g $gemos_out $gemos_err

	#Normal

	output_path="$CWD/output_fig10/probnorm/byte/gran/$g"

	KERNEL_NAME_B="$gemOS_path/gemOS_normal_$g.kernel"
	export KERNEL_NAME_B

	OUTPUT_NAME_B="$output_path"
	export OUTPUT_NAME_B

	DISK_NAME_B="$disk_path/gemos.img"
	export DISK_NAME_B

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_normal_$g.c -o run_normal_$g
	./run_normal_$g $gemos_out $gemos_err

	#Poisson

	output_path="$CWD/output_fig10/probpoisson/byte/gran/$g"

	KERNEL_NAME_B="$gemOS_path/gemOS_poisson_$g.kernel"
	export KERNEL_NAME_B

	OUTPUT_NAME_B="$output_path"
	export OUTPUT_NAME_B

	DISK_NAME_B="$disk_path/gemos.img"
	export DISK_NAME_B

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_poisson_$g.c -o run_poisson_$g
	./run_poisson_$g $gemos_out $gemos_err

	#Quick

	output_path="$CWD/output_fig10/quick/byte/gran/$g"

	KERNEL_NAME_B="$gemOS_path/gemOS_quick_$g.kernel"
	export KERNEL_NAME_B

	OUTPUT_NAME_B="$output_path"
	export OUTPUT_NAME_B

	DISK_NAME_B="$disk_path/gemos.img"
	export DISK_NAME_B

	gemos_out="$output_path/gemos.out"
	gemos_err="$output_path/gemos.err"

	gcc run_quicksort_$g.c -o run_quicksort_$g
	./run_quicksort_$g $gemos_out $gemos_err
done

#Dirtybit run

gem5_path="$CWD/gem5"
gemOS_path="$CWD/gemOS_dirtybit"
script_path="$CWD/run_scripts_fig10"
disk_path="$CWD/disk_image"
echo $gem5_path
cd $gem5_path


#switching to vanilla branch

git checkout -f vanilla


#build gem5
echo "Building gem5 for Prosper with $NUM_CPUS cpus"

scons build/X86/gem5.opt -j $NUM_CPUS


cd $script_path
cp random_db_run.sh $gem5_path 
cp run_random_db.c $gem5_path 

cp sparse_db_run.sh $gem5_path 
cp run_sparse_db.c $gem5_path 

cp stream_db_run.sh $gem5_path 
cp run_stream_db.c $gem5_path 

cp normal_db_run.sh $gem5_path 
cp run_normal_db.c $gem5_path 

cp poisson_db_run.sh $gem5_path 
cp run_poisson_db.c $gem5_path 

cp quick_db_run.sh $gem5_path 
cp run_quick_db.c $gem5_path 

cd $gemOS_path

./compile.sh random
./compile.sh sparse
./compile.sh stream
./compile.sh normal
./compile.sh poisson
./compile.sh quick

cd $gem5_path

#Random
	
output_path="$CWD/output_fig10/random/dirtybit"
	
KERNEL_NAME_B="$gemOS_path/gemOS_random.kernel"
export KERNEL_NAME_B
	
OUTPUT_NAME_B="$output_path"
export OUTPUT_NAME_B

DISK_NAME_B="$disk_path/gemos.img"
export DISK_NAME_B

gemos_out="$output_path/gemos.out"
gemos_err="$output_path/gemos.err"

gcc run_random_db.c -o run_random_db
./run_random_db $gemos_out $gemos_err


#Sparse 
	
output_path="$CWD/output_fig10/sparse/dirtybit"
	
KERNEL_NAME_B="$gemOS_path/gemOS_sparse.kernel"
export KERNEL_NAME_B
	
OUTPUT_NAME_B="$output_path"
export OUTPUT_NAME_B

DISK_NAME_B="$disk_path/gemos.img"
export DISK_NAME_B

gemos_out="$output_path/gemos.out"
gemos_err="$output_path/gemos.err"

gcc run_sparse_db.c -o run_sparse_db
./run_sparse_db $gemos_out $gemos_err

#Stream
	
output_path="$CWD/output_fig10/stream/dirtybit"
	
KERNEL_NAME_B="$gemOS_path/gemOS_stream.kernel"
export KERNEL_NAME_B
	
OUTPUT_NAME_B="$output_path"
export OUTPUT_NAME_B

DISK_NAME_B="$disk_path/gemos.img"
export DISK_NAME_B

gemos_out="$output_path/gemos.out"
gemos_err="$output_path/gemos.err"

gcc run_stream_db.c -o run_stream_db
./run_stream_db $gemos_out $gemos_err

#Normal
	
output_path="$CWD/output_fig10/probnorm/dirtybit"
	
KERNEL_NAME_B="$gemOS_path/gemOS_normal.kernel"
export KERNEL_NAME_B
	
OUTPUT_NAME_B="$output_path"
export OUTPUT_NAME_B

DISK_NAME_B="$disk_path/gemos.img"
export DISK_NAME_B

gemos_out="$output_path/gemos.out"
gemos_err="$output_path/gemos.err"

gcc run_normal_db.c -o run_normal_db
./run_normal_db $gemos_out $gemos_err

#Poisson
	
output_path="$CWD/output_fig10/probpoisson/dirtybit"
	
KERNEL_NAME_B="$gemOS_path/gemOS_poisson.kernel"
export KERNEL_NAME_B
	
OUTPUT_NAME_B="$output_path"
export OUTPUT_NAME_B

DISK_NAME_B="$disk_path/gemos.img"
export DISK_NAME_B

gemos_out="$output_path/gemos.out"
gemos_err="$output_path/gemos.err"

gcc run_poisson_db.c -o run_poisson_db
./run_poisson_db $gemos_out $gemos_err

#Quick
	
output_path="$CWD/output_fig10/quick/dirtybit"
	
KERNEL_NAME_B="$gemOS_path/gemOS_quick.kernel"
export KERNEL_NAME_B
	
OUTPUT_NAME_B="$output_path"
export OUTPUT_NAME_B

DISK_NAME_B="$disk_path/gemos.img"
export DISK_NAME_B

gemos_out="$output_path/gemos.out"
gemos_err="$output_path/gemos.err"

gcc run_quick_db.c -o run_quick_db
./run_quick_db $gemos_out $gemos_err

