#!/bin/bash

set -x

mkdir -p iso_stage || exit

echo Running mkposixdirs
"$1/mkposixdirs.sh" iso_stage || exit

echo mkposixdirs complete
ln -fsTr bootiso-bin iso_stage/bootiso-bin || exit

ln -fsTr kernel-generic iso_stage/dgos-kernel-generic || exit
ln -fsTr kernel-tracing iso_stage/dgos-kernel-tracing || exit
ln -fsTr kernel-asan iso_stage/dgos-kernel-asan || exit
for f in initrd; do
    ln -fsTr "$f" "iso_stage/$f" || exit
done

ln -fsTr "$1/user/background.png" iso_stage/background.png || exit

mkdir -p iso_stage/EFI/boot || exit
ln -fsTr bootx64.efi iso_stage/EFI/boot/bootx64.efi || exit

ln -fsTr fatpart.img iso_stage/efipart.img || exit

#cp -u bootia32.efi iso_stage/EFI/boot/bootia32.efi || exit
