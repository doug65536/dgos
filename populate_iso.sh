#!/bin/bash

TOPSRC="$1"
set -x

mkdir -p iso_stage || exit

echo Running mkposixdirs
"$TOPSRC/mkposixdirs.sh" iso_stage || exit

echo mkposixdirs complete
ln -fsr bootiso-bin iso_stage/bootiso-bin || exit

for f in *.km; do
    ln -fsTr "$f" "iso_stage/$f"
done

ln -fsr kernel-generic iso_stage/dgos-kernel-generic || exit
ln -fsr kernel-tracing iso_stage/dgos-kernel-tracing || exit
ln -fsr kernel-asan iso_stage/dgos-kernel-asan || exit
for f in initrd; do
    ln -fsr "$f" "iso_stage/$f" || exit
done

ln -fsr "$TOPSRC/user/background.png" iso_stage/usr/share/background.png || exit

mkdir -p iso_stage/EFI/boot || exit
ln -fsr bootx64.efi iso_stage/EFI/boot/bootx64.efi || exit

ln -fsr fatpart.img iso_stage/efipart.img || exit

#cp -u bootia32.efi iso_stage/EFI/boot/bootia32.efi || exit
