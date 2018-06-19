#!/bin/bash

if [[ -z "$OBJDUMP" ]]
then
	echo "Missing OBJDUMP variable"
	exit 1
fi

if [[ -z "$SORT" ]]
then
	echo "Missing SORT variable"
	exit 1
fi

objdump="$1"
type="$2"
output="$3"
input="$4"

set -x

if [[ $type == "e" ]]; then
	$OBJDUMP --syms --demangle --wide "$input" | \
		grep -P '^[0-9a-f]+\s' | \
		grep -v '^0000000000000000' | \
		grep -vP '^[0-9a-f]+\s.....d' | \
		grep -vP '\*ABS\*' |
		sed -E 's/^([0-9a-f]+)\s.{7}\s\S+\s+\S+\s+(.hidden )?(.*)/\1 \3/' | \
		$SORT > "$output"
elif [[ $type == "p" ]]; then
	$OBJDUMP --syms --demangle --wide "$input" | \
		grep -P '0x[0-9a-f]{8,16}\s' | \
		grep -v '^0000000000000000' | \
		grep -vP '\(sec\s+-\d+\)' | \
		grep -oP '(?<= 0x)[0-9a-f]{8,16} .*$' |
		$SORT > "$output"
else
	printf "Invalid type argument, must be p for PE or e for ELF\n"
	exit 1
fi
