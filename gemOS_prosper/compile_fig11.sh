#!/bin/bash

bench=$1
gran=$2
CWD=$(pwd)

if [[ $# -ne 2 ]];then
	echo "pass micro, stream, sparse, random"
	echo "pass gran"
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
    if [[ $3 == 8 ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=8 -DLOG_TRACK_GRAN=3 -DCHECKPOINT_INTERVAL=30000000"
    elif [[ $3 == 16 ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=16 -DLOG_TRACK_GRAN=4 -DCHECKPOINT_INTERVAL=30000000"
    elif [[ $3 == 32 ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=32 -DLOG_TRACK_GRAN=5 -DCHECKPOINT_INTERVAL=30000000"
    elif [[ $3 == 64 ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=64 -DLOG_TRACK_GRAN=6 -DCHECKPOINT_INTERVAL=30000000"
    elif [[ $3 == 128 ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=128 -DLOG_TRACK_GRAN=7 -DCHECKPOINT_INTERVAL=30000000"
    elif [[ $3 == "1ms" ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=8 -DLOG_TRACK_GRAN=3 -DCHECKPOINT_INTERVAL=3000000"
    elif [[ $3 == "5ms" ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=8 -DLOG_TRACK_GRAN=3 -DCHECKPOINT_INTERVAL=15000000"
    elif [[ $3 == "10ms" ]];then
        echo "gran" $3
	make "TRACKER_FLAGS=-DTRACK_SIZE=8 -DLOG_TRACK_GRAN=3 -DCHECKPOINT_INTERVAL=30000000"
    else
        echo "pass correct gran"
	exit 0
    fi
    cp gemOS.kernel $2
}


if [[ $bench == "micro" ]];then
    process "init_micro-benchmark.c" "gemOS_prosper.kernel" "$gran" 
elif [[ $bench == "sparse" ]];then
    process "ssp_sparse_init.c" "gemOS_sparse.kernel" "$gran" 
elif [[ $bench == "stream" ]];then
    process "ssp_stream_init.c" "gemOS_stream.kernel" "$gran"
elif [[ $bench == "random" ]];then
    process "ssp_random_init.c" "gemOS_random.kernel" "$gran"
elif [[ $bench == "quick" ]];then
    process "ssp_quicksort_init.c" "gemOS_quick_${gran}.kernel" "$gran"
elif [[ $bench == "normal" ]];then
    process "ssp_normal_init.c" "gemOS_normal_${gran}.kernel" "$gran"
elif [[ $bench == "poisson" ]];then
    process "ssp_poisson_init.c" "gemOS_poisson_${gran}.kernel" "$gran"
elif [[ $bench == "recursive4" ]];then
    process "ssp_recursive4_init.c" "gemOS_recursive4_${gran}.kernel" "$gran"
elif [[ $bench == "recursive8" ]];then
    process "ssp_recursive8_init.c" "gemOS_recursive8_${gran}.kernel" "$gran"
elif [[ $bench == "recursive16" ]];then
    process "ssp_recursive16_init.c" "gemOS_recursive16_${gran}.kernel" "$gran"
else
    echo "pass correct benchmark name"
    exit 0
fi

