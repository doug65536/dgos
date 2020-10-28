#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p stage || exit
"$TOPSRC/mkposixdirs.bash" stage || exit

echo Populating FAT image
mcopy -v -s -Q -i "$IMG" stage/* ::/ || exit
#mcopy -v -s -Q -i "$IMG" stage/.b* ::/ || exit
