#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p stage || exit
"$TOPSRC/mkposixdirs.sh" stage || exit

cp -u kernel-generic stage/dgos-kernel-generic || exit
cp -u kernel-bmi stage/dgos-kernel-bmi || exit
cp -u hello.km stage/hello.km || exit

cp -u user-shell stage || exit
cp -u "$TOPSRC/user/background.png" stage || exit

echo Populating FAT image
mcopy -i "$IMG" stage/* ::/ || exit
