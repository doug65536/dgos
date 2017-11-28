#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p stage
"$TOPSRC/mkposixdirs.sh" stage

cp -u kernel-generic stage/dgos-kernel-generic
cp -u kernel-sse4 stage/dgos-kernel-sse4
cp -u kernel-avx2 stage/dgos-kernel-avx2

cp -u "$TOPSRC/user/background.png" stage

echo Populating FAT image
mcopy -i "$IMG" stage/* ::/ || exit
