#!/bin/bash

echo "================================================="
echo " "
echo "         Arbitration simplification"
echo "                by Jianyi Cheng"
dt=$(date '+%d/%m/%Y %H:%M:%S');
echo "            $dt"
echo " "
echo "================================================="

# to use: bash quick_start.sh $NUMBER_OF_PARTITION $BENCHMARK
# e.g. bash quick_start.sh 08 histogram
LEGUP=/home/vagrant/legup/
PN=${1?Error: no partition given}
BN=${2:-histogram}
CURR_DIR=$PWD

# Generating Boogie progran from original code
cd $LEGUP/examples/partition/$BN/partition$PN/
sed --in-place '/REPLICATE_PTHREAD_FUNCTIONS/d' config.tcl
make
$LEGUP/llvm/Release+Asserts/bin/opt -load=EASY.so -EASY -S $BN.bc > sliced.ll
cp op.bpl $CURR_DIR

# Generating input code
echo "set_parameter REPLICATE_PTHREAD_FUNCTIONS 1" >> config.tcl
make
cp $BN.ll $CURR_DIR/input.ll


# Proving arbiter needs using Boogie
cd $CURR_DIR
./boogie_run

# Simplifying the arbiters
python EASY.py

# Generating hardware
cd $LEGUP/examples/partition/$BN/partition$PN/
cp $CURR_DIR/output.ll $BN.ll
llvm-as $BN.ll
make Backend

