#!/bin/bash

bench=$1
CWD=$(pwd)

if [[ $# -ne 1 ]];then
	echo "pass micro, stream, sparse, random"
	exit 1
fi

process() {
    echo "file" $1
    echo "output" $2
    cd user
    if [[ ! -f $1 ]];then
	    echo "file $1 not present"
	    exit 0
    fi
    cp $1 init.c
    cd $CWD
    make clean
    make
    cp gemOS.kernel $2
}


if [[ $bench == "micro" ]];then
    process "init_micro-benchmark.c" "gemOS_dirtybit.kernel"
elif [[ $bench == "sparse" ]];then
    process "ssp_sparse_init.c" "gemOS_sparse.kernel" 
elif [[ $bench == "stream" ]];then
    process "ssp_stream_init.c" "gemOS_stream.kernel" 
elif [[ $bench == "random" ]];then
    process "ssp_random_init.c" "gemOS_random.kernel" 
elif [[ $bench == "quick" ]];then
    process "ssp_quicksort_init.c" "gemOS_quick.kernel" 
elif [[ $bench == "normal" ]];then
    process "ssp_normal_init.c" "gemOS_normal.kernel" 
elif [[ $bench == "poisson" ]];then
    process "ssp_poisson_init.c" "gemOS_poisson.kernel" 
else
    echo "pass correct benchmark name"
fi

