#!/usr/bin/env sh

# Run parse_dryrun with required environment when AddressSanitizer is enabled

command="bundle exec ruby"

# Detect if extension has been compiled with AddressSanitizer
if [ -z "$OX_ASAN" ]; then
    root_dir=`dirname $0`/..
    if ldd $root_dir/lib/ox/ox.so | grep -q asan; then
        OX_ASAN=true
    fi
fi

if [ -n "$OX_ASAN" ]; then
    export LD_PRELOAD=`gcc -print-file-name=libasan.so`
fi

exec $command `dirname $0`/parse_dryrun.rb $@
