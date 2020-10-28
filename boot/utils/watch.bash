#!/bin/bash

if [ "$1" == "" ]
then
	TARGET=debug-bochs
else
	TARGET=$1
fi

if pwd | grep 'utils$'
then
	cd ..
fi

LASTMOD=0
CHILDPID=0
while true
do
	THISMOD=$(stat -c %Y ../kernel/bin/kernel)

	if [ $LASTMOD -ne $THISMOD ]
	then
		LASTMOD=$THISMOD

		if [ $CHILDPID -ne 0 ]
		then
			kill $CHILDPID
		fi

		#make debuggable-kernel-disk
		nice -n 19 make $TARGET &
		CHILDPID=$!
	else
		make kernel > /dev/null
	fi

	sleep 1
done
