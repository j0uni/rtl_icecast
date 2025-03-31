#!/bin/bash
set -ex

echo "=== Build Environment ==="
pwd
ls -la
env

echo "=== Source Files ==="
for f in Makefile config.cpp config.h rtl_icecast.cpp; do
    echo "=== $f ==="
    if [ -f "$f" ]; then
        ls -l "$f"
        echo "First few lines:"
        head -n 3 "$f"
    else
        echo "ERROR: File $f not found!"
    fi
done

echo "=== System Information ==="
gcc --version
make --version
pkg-config --list-all | grep -E 'rtlsdr|shout|lame|liquid|fftw3'

echo "=== Building Project ==="
make VERBOSE=1

echo "=== Build Results ==="
if [ -f build/rtl_icecast ]; then
    ls -l build/rtl_icecast
    file build/rtl_icecast
    ldd build/rtl_icecast
else
    echo "ERROR: Build failed - rtl_icecast binary not found"
    exit 1
fi
