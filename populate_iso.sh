#!/bin/bash
mkdir -p iso_stage || exit
"$1/mkposixdirs.sh" iso_stage || exit

cp -u bootiso-bin iso_stage/bootiso-bin || exit

cp -u kernel-generic iso_stage/dgos-kernel-generic || exit
cp -u kernel-sse4 iso_stage/dgos-kernel-sse4 || exit
cp -u kernel-avx2 iso_stage/dgos-kernel-avx2 || exit

cp -u "$1/user/background.png" iso_stage || exit
