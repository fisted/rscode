#!/bin/sh
make clean
make distclean
rm -f Makefile.in
rm -f aclocal.m4
rm -rf autom4te.cache/
rm -rf build-aux
rm -rf m4
rm -f config.h.in
rm -f configure
rm -f src/Makefile.in
