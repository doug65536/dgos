#!/bin/bash

TOPSRC="$1"
set -x

mkdir -p iso_stage || exit

echo Running mkposixdirs
"$TOPSRC/mkposixdirs.sh" iso_stage || exit
