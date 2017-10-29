#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p stage
"$TOPSRC/mkposixdirs.sh" stage
cp -u kernel-elf stage/dgos-kernel
cp -u "$TOPSRC/user/background.png" stage

echo Populating FAT image
mcopy -i "$IMG" stage/* ::/ || exit
