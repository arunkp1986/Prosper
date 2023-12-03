#!/bin/bash


echo "calling make"

make clean

make

echo "calling copy"

cp gemOS.kernel /home/kparun/Documents/thesis/gem5/fullsystem_images/

echo "done"
