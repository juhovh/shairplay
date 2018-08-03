if WITH_PLAYFAIR
PLAYFAIR_SUBDIR = playfair
PLAYFAIR_LIBADD = playfair/libplayfair.la
FAIRPLAY_SOURCE = fairplay_playfair.c
else
FAIRPLAY_SOURCE = fairplay_dummy.c
endif

SUBDIRS = crypto alac curve25519 ed25519 $(PLAYFAIR_SUBDIR)

AM_CPPFLAGS = -I$(top_srcdir)/include/shairplay

lib_LTLIBRARIES = libshairplay.la
libshairplay_la_SOURCES = base64.c base64.h digest.c digest.h dnssd.c dnssdint.h http_parser.c http_parser.h http_request.c http_request.h http_response.c http_response.h httpd.c httpd.h logger.c logger.h netutils.c netutils.h raop.c raop_buffer.c raop_buffer.h raop_rtp.c raop_rtp.h rsakey.c rsakey.h rsapem.c rsapem.h sdp.c sdp.h aes_ctr.c aes_ctr.h pairing.c pairing.h utils.c utils.h $(FAIRPLAY_SOURCE) fairplay.h plist.c plist.h compat.h memalign.h sockets.h threads.h
libshairplay_la_CPPFLAGS = $(AM_CPPFLAGS)

# This library depends on 3rd party libraries
libshairplay_la_LIBADD = crypto/libcrypto.la alac/libalac.la curve25519/libcurve25519.la ed25519/libed25519.la $(PLAYFAIR_LIBADD)
libshairplay_la_LDFLAGS = -no-undefined -version-info 0:0:0

### Update -version-info above with the following rules
# 1. Start with version information of ‘0:0:0’ for each libtool library.
# 2. Update the version information only immediately before a public release of
#    your software. More frequent updates are unnecessary, and only guarantee
#    that the current interface number gets larger faster.
# 3. If the library source code has changed at all since the last update, then
#    increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# 4. If any interfaces have been added, removed, or changed since the last
#    update, increment current, and set revision to 0.
# 5. If any interfaces have been added since the last public release, then
#    increment age.
# 6. If any interfaces have been removed or changed since the last public
#    release, then set age to 0. 
###

libshairplay_la_LIBADD += $(LIBADD_DLOPEN)
libshairplay_la_LIBADD += $(LIBM)

if OS_WIN32
libshairplay_la_CPPFLAGS += -DDLL_EXPORT
libshairplay_la_LDFLAGS += -lws2_32 -lwinmm
else
libshairplay_la_LDFLAGS += -export-symbols-regex '^(raop|dnssd)'
endif
