#!/bin/bash

export PEEPDIR=`pwd`

echo -e "\033[93m"
echo "====================================="
echo "  Building Compiler"
echo "====================================="
echo -e -n "\033[0m"

./clean.sh
make -C JOOSA-src

for BENCH_DIR in PeepholeBenchmarks/*/; do
	BENCH=$(basename $BENCH_DIR)
	echo -e "\033[93m"
	echo "====================================="
	echo "  Generating Bytecode for '$BENCH'"
	echo "====================================="
	echo -e -n "\033[0m"

	echo -e "\033[32m-------------"
	echo "  Normal"
	echo "-------------"
	echo -e -n "\033[0m"
	make -C $BENCH_DIR

	echo
	echo -e "\033[32m-------------"
	echo "  Optimized"
	echo "-------------"
	echo -e -n "\033[0m"
	make -C $BENCH_DIR opt

	echo
	echo -e "\033[32m-------------"
	echo "  Size"
	echo "-------------"
	echo -e -n "\033[0m"
	NORMAL=$(grep -a code_length $BENCH_DIR*.dump | awk '{sum += $3} END {print sum}')
	OPT=$(grep -a code_length $BENCH_DIR*.optdump | awk '{sum += $3} END {print sum}')

	echo -e "\e[41m\033[1mNormal:\033[0m\e[41m $NORMAL\e[49m"
	echo -e "\e[41m\033[1mOptimized:\033[0m\e[41m $OPT\e[49m"
done

echo -e "\033[93m"
echo "====================================="
echo "  Overall Bytecode Size"
echo "====================================="
echo -e -n "\033[0m"

NORMAL=$(grep -a code_length PeepholeBenchmarks/bench*/*.dump | awk '{sum += $3} END {print sum}')
echo -e "\e[41m\033[1mNormal:\033[0m\e[41m $NORMAL\e[49m"

OPT=$(grep -a code_length PeepholeBenchmarks/bench*/*.optdump | awk '{sum += $3} END {print sum}')
echo -e "\e[41m\033[1mOptimized:\033[0m\e[41m $OPT\e[49m"
