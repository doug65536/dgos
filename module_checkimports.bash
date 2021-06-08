#!/bin/bash

objdump_args=
if [[ "$1" == "-C" ]]; then
    objdump_args="$1"
    shift
fi

if [[ -z "$1" ]]; then
    echo Missing module filename
    exit 1
fi

if [[ -z "$OBJDUMP" ]]; then
    OBJDUMP=objdump
fi

modules="$@"

scriptroot="$(cd "$(dirname "$0")" && pwd)"

declare -A kernel_syms

while IFS= read -r line; do
    kernel_syms+=([$line]=1)
done < <("$scriptroot/kernel_exports.bash" $objdump_args)

echo Found ${#kernel_syms[@]} kernel symbols...

declare -a needed
declare -A syms

failed=
for module in $modules; do
    echo Checking $module...

    needed=()

    # Get the needed modules from the dynamic section using objdump
    while IFS= read -r line; do
        needed+=("$line")
    done < <("$OBJDUMP" -p "$module" |
        grep -oP '(?<=NEEDED)\s+\S+\.km' |
        grep -oP '\S.*')

    syms=()

    for sym in "${!kernel_syms[@]}"; do
        syms+=([$sym]=1)
    done

    for need in "${needed[@]}"; do
        echo ...needs $need
        while IFS= read -r line; do
            syms+=([$line]=1)
        done < <("$scriptroot/kernel_exports.bash" $objdump_args "$need")
    done

    #echo ${#syms[@]} symbols available

    msg=
    while IFS= read -r line; do
        if [[ -z "${syms[$line]}" ]]; then
            if [[ -z "$msg" ]]; then
                echo error: Module $module failed
                msg=1
            fi
            echo Import failure: "$line"
            failed=1
        fi
    done < <("$scriptroot/module_imports.bash" $objdump_args "$module")
done

if [[ -n "$failed" ]]; then
    echo errors! failed
    exit 1
else
    echo good
fi
