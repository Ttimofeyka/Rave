#!/bin/sh

echo "Compile i686-linux rt? (y - yes, n - no)"
read yesorno

if [[ "$yesorno" == "y" ]]; then
    cd i686-linux
    i686-elf-as rt.s -o rt.o
    i686-elf-as begin.s -o begin.o
    cd ../
fi

echo "Compile x86_64-linux rt? (y - yes, n - no)"
read yesorno

if [[ "$yesorno" == "y" ]]; then
    cd x86_64-linux
    as rt.s -o rt.o
    as begin.s -o begin.o
fi