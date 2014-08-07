#!/bin/sh
#
#  Using 'git clone' to acquire a source tree results in
#  many of the files examined by automake having equal
#  timestamps, which prompts make to attempt an
#  autoreconf which invokes the autoconf developer
#  tools.  If these aren't present, or are outdated,
#  on the user's system, *problems* will occur.
#
#  To prevent this, the present script ensures that make
#  does not attempt to rebuild the autoconf scripts when
#  it should be building PeakMem.
#
sleep 1
# aclocal-generated aclocal.m4 depends on locally-installed
# '.m4' macro files, as well as on 'configure.ac'
touch aclocal.m4
sleep 1
# autoconf-generated configure depends on aclocal.m4 and on
# configure.ac
touch configure
# so does autoheader-generated config.h.in
touch config.h.in
# and all the automake-generated Makefile.in files
touch `find . -name Makefile.in -print`
