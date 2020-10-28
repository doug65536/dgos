#!/bin/bash

TOPSRC="$1"
set -x

mkdir -p iso_stage || exit

echo Running mkposixdirs
"$TOPSRC/mkposixdirs.bash" iso_stage || exit
