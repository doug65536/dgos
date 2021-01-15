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

#sed -E 's/^([0-9a-f]+)\s.{7}\s\S+\s+\S+\s+(.hidden )?(.*)/\1 \3/' |
#grep -P '^[0-9a-fA-F]+\s.{7}\s' | \
#grep -v '^0+\s' | \
#grep -vP '^[0-9a-fA-F]+\s.....d' | \
if [[ $type == "e" ]]; then
	"$OBJDUMP" --syms --demangle --wide "$input" | \
		grep -P '^([0-9a-fA-F]+)\s.{7}\s\S+\s+\S+\s+(.hidden )?(.*)' | \
		sed -E 's/^([0-9a-fA-F]+)\s.{7}\s\S+\s+\S+\s+(.hidden )?(.*)/\1 \3/' | \
		grep -vP '\*ABS\*' |
		$SORT -u > "$output"
elif [[ $type == "p" ]]; then
	"$OBJDUMP" --syms --demangle --wide "$input" | \
		grep -P '0x[0-9a-fA-F]{8,16}\s' | \
		grep -v '^0+\s' | \
		grep -vP '\(sec\s+-\d+\)' | \
		grep -oP '(?<= 0x)[0-9a-fA-F]{8,16} .*$' |
		$SORT -u > "$output"
else
	printf "Invalid type argument, must be p for PE or e for ELF\n"
	exit 1
fi
