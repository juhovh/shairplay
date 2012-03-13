CFLAGS:=-g -Wall -Iinclude/
LDFLAGS:=-lm
LIB_OBJS=src/alac/alac.o src/crypto/aes.o src/crypto/bigint.o src/crypto/hmac.o src/crypto/md5.o src/crypto/rc4.o src/crypto/sha1.o src/sdp.o src/raop_buffer.o src/raop_rtp.o src/http_response.o src/http_request.o src/http_parser.o src/httpd.o src/raop.o src/rsakey.o src/rsapem.o src/dnssd.o src/netutils.o src/utils.o src/base64.o src/logger.o


all: example

example: test/example.o $(LIB_OBJS)
	$(CC) $(CFLAGS) test/example.o $(LIB_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f example test/*.o $(LIB_OBJS)
