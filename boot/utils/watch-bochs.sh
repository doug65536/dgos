#!/bin/bash

if pwd | grep 'utils$'
then
	cd ..
fi

LASTMOD=0
BOCHSPID=0
while true
do
	THISMOD=$(stat -c %Y ../kernel/bin/kernel)

	if [ $LASTMOD -ne $THISMOD ]
	then
		LASTMOD=$THISMOD

		if [ $BOCHSPID -ne 0 ]
		then
			kill $BOCHSPID
		fi

		make debuggable-kernel-disk
		nice -n 19 make debug-bochs &
		BOCHSPID=$!
	else
		make kernel
	fi

	sleep 1
done
