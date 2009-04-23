#! /bin/sh -e
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
  cd "$srcdir"
  export AUTOPOINT=glib-gettextize
  intltoolize --copy --force
  autoreconf --force --install
)
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
