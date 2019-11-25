#!/bin/bash

set -x

mkdir -p iso_stage || exit

echo Running mkposixdirs
"$1/mkposixdirs.sh" iso_stage || exit

echo mkposixdirs complete
cp bootiso-bin iso_stage/bootiso-bin || exit

ln -fsr kernel-generic iso_stage/dgos-kernel-generic || exit
ln -fsr kernel-tracing iso_stage/dgos-kernel-tracing || exit
ln -fsr kernel-asan iso_stage/dgos-kernel-asan || exit
for f in initrd; do
    ln -fsr "$f" "iso_stage/$f" || exit
done

ln -fsr "$1/user/background.png" iso_stage/background.png || exit

mkdir -p iso_stage/EFI/boot || exit
ln -fsr bootx64.efi iso_stage/EFI/boot/bootx64.efi || exit

ln -fsr fatpart.img iso_stage/efipart.img || exit

#cp -u bootia32.efi iso_stage/EFI/boot/bootia32.efi || exit
