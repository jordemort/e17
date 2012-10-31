#!/bin/sh

autoreconf --symlink --install -Wno-portability

if [ -z "$NOCONFIGURE" ]; then
	exec ./configure -C "$@"
fi
