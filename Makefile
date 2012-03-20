CFLAGS:=-g -Wall -Isrc/include/
LDFLAGS:=-lm
LIB_OBJS=src/lib/alac/alac.o src/lib/crypto/aes.o src/lib/crypto/bigint.o src/lib/crypto/hmac.o src/lib/crypto/md5.o src/lib/crypto/rc4.o src/lib/crypto/sha1.o src/lib/sdp.o src/lib/raop_buffer.o src/lib/raop_rtp.o src/lib/digest.o src/lib/http_response.o src/lib/http_request.o src/lib/http_parser.o src/lib/httpd.o src/lib/raop.o src/lib/rsakey.o src/lib/rsapem.o src/lib/dnssd.o src/lib/netutils.o src/lib/utils.o src/lib/base64.o src/lib/logger.o


all: example

example: src/test/example.o $(LIB_OBJS)
	$(CC) $(CFLAGS) src/test/example.o $(LIB_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f example src/test/*.o $(LIB_OBJS)
