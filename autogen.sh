#!/bin/sh
set -e
[ -d m4 ] || mkdir m4 || exit
autoreconf --install --force
rm -f config.sub.new
sed -E 's/( |\-)linux\-uclibc\*/\1linux\-uc\*/g' config.sub > config.sub.new
chmod a+x config.sub.new
mv -f config.sub.new config.sub
