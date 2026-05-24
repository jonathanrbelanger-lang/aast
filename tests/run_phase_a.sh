#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "--- Compiling Phase A Harness ---"
# Compile with strict warnings and debugging symbols
gcc -Wall -Wextra -g test_phase_a.c aast.c -o test_phase_a -lcrypto

echo "--- Running Valgrind Profiler ---"
# Run valgrind and pipe output to a log file
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --error-exitcode=1 \
         --log-file=valgrind_phase_a.log \
         ./test_phase_a

echo "--- Parsing Results ---"
# Check for the holy grail of C programming
if grep -q "All heap blocks were freed -- no leaks are possible" valgrind_phase_a.log; then
    echo "[PASS] Zero memory leaks detected."
else
    echo "[FAIL] Memory leak detected! Check valgrind_phase_a.log"
    exit 1
fi

if grep -q "ERROR SUMMARY: 0 errors from 0 contexts" valgrind_phase_a.log; then
    echo "[PASS] Zero memory errors detected."
else
    echo "[FAIL] Memory errors detected! Check valgrind_phase_a.log"
    exit 1
fi

echo "Phase A Baseline successfully mapped."
