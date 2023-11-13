#!/bin/bash
echo $OUTPUT_NAME_C
echo $KERNEL_NAME_C
echo $DISK_NAME_C

./run_nobypass.sh $OUTPUT_NAME_C $KERNEL_NAME_C $DISK_NAME_C > quick_1ms.out 2>&1
