#!/bin/tcsh

# memory leak checking
#valgrind --max-stackframe=4194736 --track-origins=yes --leak-check=full --show-leak-kinds=all ./rl0b_main_debug -skipRecord packetizer_out_2024_09_25.bin

# performance
valgrind --tool=callgrind --cache-sim=yes --branch-sim=yes --max-stackframe=4194736 ./rl0b_main -skipRecord packetizer_out_2024_09_25.bin
