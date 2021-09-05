#!/bin/sh

cd "$(dirname $0)"

gcc -Wall -Ofast *.c "$@"
