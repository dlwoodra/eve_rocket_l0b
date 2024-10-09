#!/bin/tcsh

make -f Makefile clean
time make -j2 -f Makefile rl0b_main_debug
make -f Makefile clean

