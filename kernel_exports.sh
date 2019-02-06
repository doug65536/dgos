#!/bin/bash

objdump_args=
if [[ "$1" == "-C" ]]; then
    objdump_args="$1"
    shift
fi

src="$1"
if [[ -z "$1" ]]; then
    src=kernel-generic
fi

x86_64-elf-objdump --wide --dynamic-syms $objdump_args $src | \
    grep -oP '(.text|.data|.bss|.rodata).*' | \
    sed -E 's/^\S+\s+\S+\s+//g'
