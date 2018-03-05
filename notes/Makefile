
CC_GCC=gcc
GCC_FLAGS=-O2 -Wall
GCC_SO_FLAGS=-shared
#If need to expose all symbols, use: -rdynamic

CC_CLANG=./llvm-riscv-toolchain/bin/clang
CLANG_FLAGS=-O2 -Wall -S -emit-llvm

CC_LLC=./llvm-riscv-toolchain/bin/llc
LLC_FLAGS=-O2 -relocation-model=pic -filetype=obj
# Uncomment to override arch during JIT compile step
#LLC_TARGET=-march=ppc32

DL_LIB=-ldl

DP_DEPS=main-dl.c key.h util.h test_packets.h
EXTRACT_DEPS=extract2.c key.h util.h

# Quick test performs each on this machine:
all: main-dl extract-ir extract-lib

# Steps 1-3 are inteded to be run seperately on different machines.
# - Be sure to update paths to clang, llc, and gcc
# - Be sure to adjust LLC_TARGET on target machine to override triple in
#   the LLVM IR (*.ll) file.

# Step 1: Compile main normally with support for Dynamic Loading of Libs
#main: main-dl

# Step 2: Generate LLVM IR and transfer to target machine
extract-ir: extract-2.1.ll

# Step 3: JIT compile LLVM IR to shared object library
extract-lib: extract-2.1.so


main-dl: $(DP_DEPS)
	$(CC_GCC) $(GCC_FLAGS) main-dl.c -o main-dl $(DL_LIB)

extract-2.1.so: extract-2.1.o
	$(CC_GCC) $(GCC_SO_FLAGS) extract-2.1.o -o extract-2.1.so

extract-2.1.o: extract-2.1.ll
	$(CC_LLC) $(LLC_FLAGS) $(LLC_TARGET) extract-2.1.ll -o extract-2.1.o

extract-2.1.ll: $(EXTRACT_DEPS)
	$(CC_CLANG) $(CLANG_FLAGS) extract2.c -o extract-2.1.ll

clean:
	rm -f extract-2.1.o extract-2.1.ll extract-2.1.so main-dl

