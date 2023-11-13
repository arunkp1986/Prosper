#!/bin/bash
echo $OUTPUT_NAME_B
echo $KERNEL_NAME_B
echo $DISK_NAME_B

./run_nobypass.sh $OUTPUT_NAME_B $KERNEL_NAME_B $DISK_NAME_B > normal_db.out 2>&1
