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
    rm -f tests/test_phase_a tests/*.log

# --- Test Suite ---

# Compile the Phase A test binary directly into the tests/ directory
tests/test_phase_a: tests/test_phase_a.c aast.c
	$(CC) $(CFLAGS) -I. $^ -o $@ -lcrypto

# Execute the automated Valgrind gauntlet
test_a: tests/test_phase_a
	@echo "Executing Phase A Constraint Mapping..."
	@cd tests && bash run_phase_a.sh
