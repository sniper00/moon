#!/usr/bin/env bash
### configure C compiler
export compiler=$(which gcc)
### get version code
MAJOR=$(echo __GNUC__ | $compiler -E -xc - | tail -n 1)
MINOR=$(echo __GNUC_MINOR__ | $compiler -E -xc - | tail -n 1)
PATCHLEVEL=$(echo __GNUC_PATCHLEVEL__ | $compiler -E -xc - | tail -n 1)

if (($MAJOR<9)); then
    echo "require gcc 9.x or higher"
    exit
fi

echo "use gcc " $MAJOR"."$MINOR 
premake5 gmake
make clean config=release
make config=release
