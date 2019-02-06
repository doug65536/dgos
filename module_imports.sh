#!/bin/bash

objdump_args=
if [[ "$1" == "-C" ]]; then
    objdump_args="$1"
    shift
fi

src="$1"
if [[ -z "$1" ]]; then
    echo Missing module filename argument
    exit 1
fi

x86_64-elf-objdump --wide --dynamic-syms $objdump_args $src | \
    grep -oP '\*UND\*.*' | \
    sed -E 's/^\S+\s+\S+\s+//g'
