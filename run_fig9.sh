#!/bin/bash

echo "Running SSP"
./run_fig9_ssp.sh

echo "Running SSP-Dirtybit"
./run_fig9_ssp_dirtybit.sh

echo "Running SSP-Prosper"
./run_fig9_ssp_prosper.sh

echo "Running Vanilla"
./run_fig9_vanilla.sh
