#!/bin/bash
echo $OUTPUT_NAME
echo $KERNEL_NAME
echo $DISK_NAME

./run_nobypass.sh $OUTPUT_NAME $KERNEL_NAME $DISK_NAME > stream.out 2>&1
