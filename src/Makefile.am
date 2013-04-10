SUBDIRS = lib

AM_CPPFLAGS = -I$(top_srcdir)/include


if WITH_SHAIRPLAY

bin_PROGRAMS = shairplay
shairplay_SOURCES = shairplay.c
shairplay_LDADD = lib/libshairplay.la
shairplay_CFLAGS =
shairplay_LDFLAGS = -static-libtool-libs

if HAVE_AVAHI
shairplay_SOURCES += mdns_avahi.c
shairplay_CFLAGS += $(avahi_CFLAGS)
shairplay_LDADD += $(avahi_CFLAGS)
else
shairplay_SOURCES += mdns_dnssd.c
endif

shairplay_CFLAGS += $(libao_CFLAGS)
shairplay_LDADD += $(libao_LIBS)

endif
