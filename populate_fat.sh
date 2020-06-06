#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p stage || exit
"$TOPSRC/mkposixdirs.sh" stage || exit

truncate --size 128K stage/.big || exit

for f in *.km; do
    ln -fsTr "$f" "stage/$f"
done

ln -fsTr kernel-generic stage/dgos-kernel-generic || exit
ln -fsTr kernel-tracing stage/dgos-kernel-tracing || exit
ln -fsTr kernel-asan stage/dgos-kernel-asan || exit
for f in initrd; do
    ln -fsTr "$f" "stage/$f" || exit
done

ln -fsTr "$TOPSRC/user/background.png" stage/background.png || exit

mkdir -p stage/sym || exit
for f in sym/*; do
    ln -fsTr "$f" "stage/$f" || exit
done

# EFI boot files
mkdir -p stage/EFI/boot || exit
ln -fsTr bootx64.efi stage/EFI/boot/bootx64.efi || exit
#ln -fsTr bootia32.efi stage/EFI/boot/bootia32.efi || exit

echo Populating FAT image
mcopy -v -s -Q -i "$IMG" stage/* ::/ || exit
mcopy -v -s -Q -i "$IMG" stage/.b* ::/ || exit
