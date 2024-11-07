#!/bin/tcsh

## run perf first
sudo sysctl -w kernel.perf_event_paranoid=1
perf record -g ./rl0b_main_gui -skipRecord packetizer_out_2024_10_23.bin
sudo sysctl -w kernel.perf_event_paranoid=4
perf report
exit 0


# memory leak checking
#valgrind --suppressions=valgrind.suppressions --max-stackframe=4194736 --track-origins=yes --leak-check=full --show-leak-kinds=all ./rl0b_main_gui -skipRecord packetizer_out_2024_10_23.bin >& valgrind_leaks.log
valgrind --suppressions=valgrind.suppressions --max-stackframe=5194736 --track-origins=yes --leak-check=full --show-leak-kinds=all ./rl0b_main_gui -skipRecord record_2024304_171000.rtlm >& valgrind_leaks.log

# performance
valgrind --suppressions=valgrind.suppressions --tool=callgrind --cache-sim=yes --branch-sim=yes --max-stackframe=4194736 ./rl0b_main -skipRecord packetizer_out_2024_10_23.bin >& valgrind_callgrind.log

# need to use calgrind_annotate to view the output of callgrind
#calgrind_annotate --auto=yes valgrind_callgrind.log
# of kcachegrind
#kcachegrind valgrind_callgrind.log

# race conditions
valgrind --tool=helgrind ./rl0b_main_gui -skipRecord packetizer_out_2024_10_23.bin >& valgrind_helgrind.log