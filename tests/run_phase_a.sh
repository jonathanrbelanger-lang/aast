#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define test parameters (We will isolate these later, but here is a starting block)
TEST_CHILDREN=10000
TEST_PAYLOAD=10485760 # 10 MB

echo "--- Running Valgrind Profiler ---"
# Run valgrind and pipe output to a log file, passing the arguments
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --error-exitcode=1 \
         --log-file=valgrind_phase_a.log \
         ./test_phase_a $TEST_CHILDREN $TEST_PAYLOAD

echo "--- Parsing Results ---"
if grep -q "All heap blocks were freed -- no leaks are possible" valgrind_phase_a.log; then
    echo "[PASS] Zero memory leaks detected."
else
    echo "[FAIL] Memory leak detected! Check tests/valgrind_phase_a.log"
    exit 1
fi

if grep -q "ERROR SUMMARY: 0 errors from 0 contexts" valgrind_phase_a.log; then
    echo "[PASS] Zero memory errors detected."
else
    echo "[FAIL] Memory errors detected! Check tests/valgrind_phase_a.log"
    exit 1
fi

echo "Phase A Baseline successfully mapped."
