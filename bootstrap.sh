#!/bin/sh

AUTOCONF_FILES="Makefile.in aclocal.m4 ar-lib autom4te.cache compile \
        config.guess config.h.in config.sub configure depcomp install-sh \
        ltmain.sh missing *libtool test-driver"

case $1 in
    clean)
        test -f Makefile && make maintainer-clean
        for file in ${AUTOCONF_FILES}; do
            find -name "$file" | xargs -r rm -rf
        done
        exit 0
        ;;
esac

autoreconf -i
echo 'Run "./configure ${CONFIGURE_FLAGS} && make"'
echo 'To enable LTO, run'
echo '"./configure ${CONFIGURE_FLAGS} --enable-lto \'
echo 'AR=${TARGET_PREFIX}gcc-ar RANLIB=${TARGET_PREFIX}gcc-ranlib && make"'
