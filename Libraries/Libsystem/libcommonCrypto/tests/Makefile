# Host tests for libcommonCrypto: make -C Libraries/Libsystem/libcommonCrypto/tests

CC ?= cc
CFLAGS = -O1 -g -Wall -Wextra -fsanitize=address,undefined \
	-Ishim -I.. -idirafter ../../../../Developer/ravynOS.sdk/usr/include

all: test

CRYPT_SRCS = ../cryptor.c ../aes.c ../keyderivation.c ../hmac.c ../digest.c

ccdigest: ccdigest.c ../digest.c ../hmac.c ../digest_tables.h
	$(CC) $(CFLAGS) -o $@ ccdigest.c ../digest.c ../hmac.c

cccrypt: cccrypt.c $(CRYPT_SRCS) ../aes_tables.h ../aes.h
	$(CC) $(CFLAGS) -o $@ cccrypt.c $(CRYPT_SRCS)

test: ccdigest cccrypt
	python3 run_tests.py ./ccdigest
	python3 run_crypt_tests.py ./cccrypt

clean:
	rm -f ccdigest cccrypt

.PHONY: all test clean
