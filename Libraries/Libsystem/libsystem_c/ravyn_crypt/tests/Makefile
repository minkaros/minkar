# Host test of crypt(3)'s SHA-crypt code: make -C <this directory>
# (GNU or BSD make, with the host's clang).

CC ?=		cc
LIBCRYPT =	../../../../../BSD/lib/libcrypt
CFLAGS =	-g -O1 -Wall -Wextra -fsanitize=address,undefined \
		-include ../crypt_internal.h -I.. -I../sys/.. -I$(LIBCRYPT) \
		-D'__FBSDID(x)='
SRCS =		test_crypt.c ../sha2.c ../ravyn_crypt.c $(LIBCRYPT)/crypt-sha256.c \
		$(LIBCRYPT)/crypt-sha512.c $(LIBCRYPT)/misc.c

test: test_crypt
	./test_crypt

test_crypt: $(SRCS) ../sha256.h ../sha512.h ../crypt_internal.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f test_crypt

.PHONY: test clean
