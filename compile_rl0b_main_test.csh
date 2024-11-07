#!/bin/tcsh

make -f Makefile clean
time make -j4 -f Makefile rl0b_test
make -f Makefile clean

