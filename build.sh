#!/bin/sh

CC="gcc"
CFLAGS="-O3 -g"

mkdir -p bin

for f in switch direct indirect toc; do
	$CC -o bin/$f $CFLAGS $f-interpreter.c
done

for f in bff bff4; do
	#$CC -o bin/$f $CFLAGS other/$f.c
	true
done
