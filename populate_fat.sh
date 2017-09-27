#!/bin/bash

IMG="$1"
mcopy -i "$IMG" kernel-elf ::/dgos-kernel || exit
mcopy -i "$IMG" "$2/user/background.png" ::/ || exit
