SUBDIRS = crypto alac

AM_CPPFLAGS = -I$(top_srcdir)/include/shairplay

lib_LTLIBRARIES = libshairplay.la
libshairplay_la_SOURCES = base64.c base64.h digest.c digest.h dnssd.c dnssdint.h http_parser.c http_parser.h http_request.c http_request.h http_response.c http_response.h httpd.c httpd.h logger.c logger.h netutils.c netutils.h raop.c raop_buffer.c raop_buffer.h raop_rtp.c raop_rtp.h rsakey.c rsakey.h rsapem.c rsapem.h sdp.c sdp.h utils.c utils.h compat.h memalign.h sockets.h threads.h

# This library depends on 3rd party libraries
libshairplay_la_LIBADD = crypto/libcrypto.la alac/libalac.la
libshairplay_la_LDFLAGS = -version-info 0:0:0

libshairplay_la_LIBADD += $(LIBADD_DLOPEN)
libshairplay_la_LIBADD += $(LIBM)

if OS_WIN32
libshairplay_la_LDFLAGS += -no-undefined -lws2_32 -lwinmm
endif
