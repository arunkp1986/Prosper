#!/bin/bash
echo $OUTPUT_NAME_A
echo $KERNEL_NAME_A
echo $DISK_NAME_A

./run_nobypass.sh $OUTPUT_NAME_A $KERNEL_NAME_A $DISK_NAME_A > gapbs_db_100us.out 2>&1
