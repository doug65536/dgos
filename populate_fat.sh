#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p fat_stage
"$TOPSRC/mkposixdirs.sh" fat_stage
cp -u kernel-elf fat_stage/dgos-kernel
cp -u "$TOPSRC/user/background.png" fat_stage

echo Populating FAT image
mcopy -i "$IMG" fat_stage/* ::/ || exit
