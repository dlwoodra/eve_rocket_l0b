#!/bin/tcsh

make -f Makefile clean
time make -j4 -f Makefile rl0b_main_debug
make -f Makefile clean

