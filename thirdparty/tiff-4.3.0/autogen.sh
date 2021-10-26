#!/bin/sh
set -x
libtoolize --force --copy
aclocal -I ./m4
autoheader
automake --foreign --add-missing --copy
autoconf
# Get latest config.guess and config.sub from upstream master since
# these are often out of date.
for file in config.guess config.sub
do
    echo "$0: getting $file..."
    wget -q --timeout=5 -O config/$file.tmp \
      "https://git.savannah.gnu.org/cgit/config.git/plain/${file}" \
      && mv config/$file.tmp config/$file \
      && chmod a+x config/$file
    retval=$?
    rm -f config/$file.tmp
    test $retval -eq 0 || exit $retval
done
