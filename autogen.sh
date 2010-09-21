#! /bin/sh

set -ex

aclocal
autoconf --force --warnings=all
autoheader --force --warnings=all
automake --add-missing --copy --foreign --warnings=all

