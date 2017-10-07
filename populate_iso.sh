#!/bin/bash
mkdir -p iso_stage || exit
"$1/mkposixdirs.sh" iso_stage || exit
cp -u bootiso-bin iso_stage/bootiso-bin || exit
cp -u kernel-elf iso_stage/dgos-kernel || exit
cp -u "$1/user/background.png" iso_stage || exit
