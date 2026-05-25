# =============================================================================
# Makefile for the Accretive-Abstract-State-Tree (A-AST) Project
# =============================================================================

# --- Compiler and Flags ---
# Use gcc as the C compiler
CC = gcc
# CFLAGS: Standard flags for compilation.
# -g: Include debug symbols (essential for Valgrind and GDB).
# -Wall -Wextra: Enable all standard and extra warnings for robust code.
# -std=c11: Use the C11 standard.
CFLAGS = -g -Wall -Wextra -std=c11
# LDFLAGS: Flags passed to the linker.
LDFLAGS =
# LDLIBS: Libraries to link against. -lcrypto is for OpenSSL.
LDLIBS = -lcrypto

# --- Debug Specific Flags ---
# CPPFLAGS: C Pre-Processor flags. -DDEBUG_PRINT enables the pretty-printer.
DEBUG_CPPFLAGS = -DDEBUG_PRINT

# --- File and Target Definitions ---
# The final executable for the example program.
TARGET = example_aast
# The executable for the debug version of the example program.
DEBUG_TARGET = example_aast_debug

# Source files for the library.
LIB_SRCS = aast.c
# Object files for the library.
LIB_OBJS = $(LIB_SRCS:.c=.o)

# Source file for the example program.
EXAMPLE_SRCS = example.c
# Object file for the example program.
EXAMPLE_OBJS = $(EXAMPLE_SRCS:.c=.o)


# --- Build Targets ---

# The .PHONY directive tells make that these are not actual files.
.PHONY: all debug test clean

# Default target: executed when you just run `make`.
# Builds the standard release version of the example program.
all: $(TARGET)

# Builds the debug version of the example program.
debug: $(DEBUG_TARGET)

# The release executable depends on the library and example object files.
$(TARGET): $(LIB_OBJS) $(EXAMPLE_OBJS)
	@echo "==> Linking release executable: $@"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# The debug executable is built similarly, but with the debug flag.
# We recompile the sources with the debug flag set.
$(DEBUG_TARGET): $(LIB_SRCS) $(EXAMPLE_SRCS)
	@echo "==> Compiling and linking debug executable: $@"
	$(CC) $(CFLAGS) $(DEBUG_CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Generic rule to compile any .c file into a .o file.
# $< is the source file, $@ is the target object file.
# The CFLAGS are applied here during compilation.
%.o: %.c aast.h
	@echo "==> Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# --- Utility Targets ---

# Runs the debug version of the program through Valgrind.
# It depends on the `debug` target, so it will build it first if needed.
test: debug
	@echo "==> Running Valgrind memory check..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(DEBUG_TARGET)

# Cleans up all build artifacts and generated files.
clean:
	@echo "==> Cleaning up build artifacts..."
	rm -f $(TARGET) $(DEBUG_TARGET) $(LIB_OBJS) $(EXAMPLE_OBJS) aast.dat core
	rm -f tests/test_phase_a tests/test_key_limit tests/test_depth_limit tests/*.log
	rm -f tests/test_query
	rm -f tests/test_query_scale
	rm -f tests/test_filetype
# --- Test Suite ---

# Compile the Phase A test binary directly into the tests/ directory
tests/test_phase_a: tests/test_phase_a.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ $(LDLIBS)

# Execute the automated Valgrind gauntlet
test_a: tests/test_phase_a
	@echo "Executing Phase A Constraint Mapping..."
	@cd tests && bash run_phase_a.sh
# Compile the Key Length test binary
tests/test_key_limit: tests/test_key_limit.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ $(LDLIBS)

# Run a swept loop across key sizes to find the inflection point
test_key_sweep: tests/test_key_limit
	@echo "--- Beginning Multi-Step Key Length Sweep ---"
	@./tests/test_key_limit 32
	@./tests/test_key_limit 128
	@./tests/test_key_limit 512
	@./tests/test_key_limit 2048
	@./tests/test_key_limit 8192
	@./tests/test_key_limit 32768
# Run a scaling payload sweep with zero children to isolate heap/hashing limits
test_payload_sweep: tests/test_phase_a
	@echo "--- Beginning Progressive Payload Scale Sweep ---"
	@./tests/test_phase_a 0 10485760    # 10 MB
	@./tests/test_phase_a 0 52428800    # 50 MB
	@./tests/test_phase_a 0 209715200   # 200 MB
	@./tests/test_phase_a 0 1073741824  # 1 GB
# Compile the Vertical Depth test binary
tests/test_depth_limit: tests/test_depth_limit.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ $(LDLIBS)

# Run a progressive depth sweep to find where the stack or guardrail breaks
test_depth_sweep: tests/test_depth_limit
	@echo "--- Beginning Progressive Vertical Depth Sweep ---"
	@./tests/test_depth_limit 1000      # Nominal deep tree
	@./tests/test_depth_limit 10000     # Heavy deep tree
	@./tests/test_depth_limit 50000     # Extreme deep tree

# --- Query API Tests ---

# Compile the Query test binary
tests/test_query: tests/test_query.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ $(LDLIBS)

# Run the Query test under Valgrind
test_query: tests/test_query
	@echo "--- Running Query API Test ---"
	valgrind --leak-check=full --show-leak-kinds=all ./tests/test_query


# --- Query Scale Tests ---

# Compile the Query Scale test binary
tests/test_query_scale: tests/test_query_scale.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ $(LDLIBS)

# Run a horizontal scaling sweep (1K, 10K, 100K children)
test_query_horizontal: tests/test_query_scale
	@echo "--- Beginning Query Horizontal Sweep ---"
	@./tests/test_query_scale horizontal 1000
	@./tests/test_query_scale horizontal 10000
	@./tests/test_query_scale horizontal 100000

# Run a vertical scaling sweep
test_query_vertical: tests/test_query_scale
	@echo "--- Beginning Query Vertical Sweep ---"
	@./tests/test_query_scale vertical 1000
	@./tests/test_query_scale vertical 10000
	@./tests/test_query_scale vertical 30000
	@./tests/test_query_scale vertical 50000
	@./tests/test_query_scale vertical 100000
# --- Filetype Formalization Tests ---
tests/test_filetype: tests/test_filetype.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ $(LDLIBS)

test_filetype: tests/test_filetype
	@echo "--- Running Filetype Enforcement Test ---"
	valgrind --leak-check=full --show-leak-kinds=all ./tests/test_filetype
# --- Tooling: NFC Ruleset Compiler ---
tools/build_nfc_aast: tools/build_nfc_aast.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ $(LDLIBS)

build_ucd: tools/build_nfc_aast
	@echo "1. Downloading and Extracting UCD via Python..."
	python3 tools/fetch_ucd.py
	@echo "2. Compiling and running A-AST Forge..."
	./tools/build_nfc_aast
