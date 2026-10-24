# Host tests for libcommonCrypto: make -C Libraries/Libsystem/libcommonCrypto/tests

CC ?= cc
CFLAGS = -O1 -g -Wall -Wextra -fsanitize=address,undefined \
	-Ishim -I.. -idirafter ../../../../Developer/ravynOS.sdk/usr/include

all: test

ccdigest: ccdigest.c ../digest.c ../hmac.c ../digest_tables.h
	$(CC) $(CFLAGS) -o $@ ccdigest.c ../digest.c ../hmac.c

test: ccdigest
	python3 run_tests.py ./ccdigest

clean:
	rm -f ccdigest

.PHONY: all test clean
