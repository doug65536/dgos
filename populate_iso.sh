#!/bin/bash

TOPSRC="$1"
set -x

mkdir -p iso_stage || exit

echo Running mkposixdirs
"$TOPSRC/mkposixdirs.sh" iso_stage || exit

#for f in *.km; do
#    ln -fsTr "$f" "iso_stage/boot/$f"
#done

#for f in kernel-generic kernel-tracing kernel-asan initrd; do
#    ln -fsr "$f" "iso_stage/boot/$f" || exit
#done

#ln -fsr "$TOPSRC/user/background.png" iso_stage/usr/share/background.png || exit

#mkdir -p iso_stage/EFI/boot || exit
#ln -fsr bootx64.efi iso_stage/EFI/boot/bootx64.efi || exit

# FAT image for EFI partition for EFI boot
#ln -fsr fatpart.img iso_stage/efipart.img || exit

#cp -u bootia32.efi iso_stage/EFI/boot/bootia32.efi || exit
