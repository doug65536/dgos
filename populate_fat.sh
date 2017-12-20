#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p stage || exit
"$TOPSRC/mkposixdirs.sh" stage || exit

cp -u kernel-generic stage/dgos-kernel-generic || exit
cp -u kernel-sse4 stage/dgos-kernel-sse4 || exit
cp -u kernel-avx2 stage/dgos-kernel-avx2 || exit

cp -u user-shell stage || exit
cp -u "$TOPSRC/user/background.png" stage || exit

echo Populating FAT image
mcopy -i "$IMG" stage/* ::/ || exit
