#!/bin/sh
# HarfBuzz's fuzzer regression tests (test/fuzzing/run-fuzzer-tests.py, in
# sh): the fuzzer program over every file of the directories, 64 at a time;
# it must exit 0 on each batch. Usage: fuzz.sh fuzzer dir...
fuzzer=$1; shift
for d in "$@"; do find "$d" -type f; done | xargs -n 64 "$fuzzer" > /dev/null
