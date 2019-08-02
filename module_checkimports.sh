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

modules="$@"

scriptroot="$(cd "$(dirname "$0")" && pwd)"

declare -A kernel_syms

while IFS= read -r line; do
    kernel_syms+=([$line]=1)
done < <("$scriptroot/kernel_exports.sh" $objdump_args)

echo Found ${#kernel_syms[@]} kernel symbols...

failed=
for module in $modules; do
    echo Checking $module...
    msg=
    while IFS= read -r line; do
        if [[ -z "${kernel_syms[$line]}" ]]; then
            if [[ -z "$msg" ]]; then
                echo error: Module $module failed
                msg=1
            fi
            echo Import failure: "$line"
            failed=1
        fi
    done < <("$scriptroot/module_imports.sh" $objdump_args "$module")
done

if [[ -n "$failed" ]]; then
    exit 1
fi
