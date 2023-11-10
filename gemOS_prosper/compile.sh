#!/bin/bash

bench=$1
#rec_count=$2
CWD=$(pwd)

if [[ $# -ne 1 ]];then
	echo "pass micro, stream, sparse, random"
	#echo "rec count, gapbs=83004358, sssp=49692076, ycsb=74286453, else 0"
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
    #if [[ $3 -gt 0 ]];then
     #   echo $3
      #  make "REC_COUNT=-DREC_COUNT="$3
    #else
     #   make
    #fi
    cp gemOS.kernel $2

}


if [[ $bench == "micro" ]];then
    process "init_micro-benchmark.c" "gemOS_prosper.kernel"
elif [[ $bench == "sparse" ]];then
    process "ssp_sparse_init.c" "gemOS_sparse.kernel" 
elif [[ $bench == "stream" ]];then
    process "ssp_stream_init.c" "gemOS_stream.kernel" 
elif [[ $bench == "random" ]];then
    process "ssp_random_init.c" "gemOS_random.kernel" 
else
    echo "pass correct benchmark name"
fi

