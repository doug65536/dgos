#!/bin/bash

set +x

mkdir -p iso_stage || exit
"$1/mkposixdirs.sh" iso_stage || exit

cp -u bootiso-bin iso_stage/bootiso-bin || exit

cp -u kernel-generic iso_stage/dgos-kernel-generic || exit
cp -u kernel-bmi iso_stage/dgos-kernel-bmi || exit
cp -u hello.km iso_stage/hello.km || exit

cp -u user-shell iso_stage || exit
cp -u "$1/user/background.png" iso_stage || exit

mkdir -p iso_stage/EFI/boot || exit
cp -u bootx64.efi iso_stage/EFI/boot/bootx64.efi || exit
#cp -u bootia32.efi iso_stage/EFI/boot/bootia32.efi || exit
