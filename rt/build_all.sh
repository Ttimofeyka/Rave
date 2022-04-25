#!/bin/sh

cd i686-linux
as rt.s -o rt.o
as begin.s -o begin.o

cd ../i686-mac
as rt.s -o rt.o

cd ../win32
as rt.s -o rt.o
as begin.s -o begin.o

cd ../win64
as rt.s -o rt.o

cd ../x86_64-linux
as rt.s -o rt.o
as begin.s -o begin.o

cd ../x86_64-mac
as rt.s -o rt.o