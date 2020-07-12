#!/bin/bash

[[ -z "$1" ]] && exit 1

mkdir -p "$1/bin" || exit
mkdir -p "$1/boot" || exit
mkdir -p "$1/dev" || exit
mkdir -p "$1/etc" || exit
mkdir -p "$1/home" || exit
mkdir -p "$1/lib" || exit
mkdir -p "$1/media" || exit
mkdir -p "$1/mnt" || exit
mkdir -p "$1/opt" || exit
mkdir -p "$1/proc" || exit
mkdir -p "$1/root" || exit
mkdir -p "$1/run" || exit
mkdir -p "$1/include" || exit
mkdir -p "$1/sbin" || exit
mkdir -p "$1/srv" || exit
mkdir -p "$1/sys" || exit
mkdir -p "$1/tmp" || exit
mkdir -p "$1/usr" || exit
mkdir -p "$1/usr/bin" || exit
mkdir -p "$1/usr/include" || exit
mkdir -p "$1/usr/local" || exit
mkdir -p "$1/usr/sbin" || exit
mkdir -p "$1/usr/share" || exit
mkdir -p "$1/usr/src" || exit
mkdir -p "$1/var" || exit
mkdir -p "$1/var/cache" || exit
mkdir -p "$1/var/lib" || exit
mkdir -p "$1/var/lock" || exit
mkdir -p "$1/var/log" || exit
mkdir -p "$1/var/mail" || exit
mkdir -p "$1/var/opt" || exit
mkdir -p "$1/var/run" || exit
mkdir -p "$1/var/spool" || exit
mkdir -p "$1/var/tmp" || exit
mkdir -p "$1/dev" || exit
touch "$1/home/unicode-🂡-works"
