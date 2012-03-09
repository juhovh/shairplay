#-------------------------------------------------
#
# Project created by QtCreator 2011-12-04T00:47:51
#
#-------------------------------------------------

QT       += core gui multimedia phonon svg

include(qtsingleapplication/src/qtsingleapplication.pri)

TARGET = AirTV
TEMPLATE = app
ICON = AirTV.icns
RC_FILE = AirTV.rc

win32 {
    LIBS += C:\\QtSDK\\mingw\\lib\\libws2_32.a
    QMAKE_LFLAGS += -static-libgcc
}
unix:!macx {
    LIBS += -ldns_sd
}
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
}

INCLUDEPATH += ../include/
SOURCES += main.cpp\
    ../src/utils.c \
    ../src/sdp.c \
    ../src/rsapem.c \
    ../src/rsakey.c \
    ../src/raop.c \
    ../src/raop_rtp.c \
    ../src/raop_buffer.c \
    ../src/netutils.c \
    ../src/httpd.c \
    ../src/http_response.c \
    ../src/http_request.c \
    ../src/http_parser.c \
    ../src/dnssd.c \
    ../src/base64.c \
    ../src/alac/alac.c \
    ../src/crypto/sha1.c \
    ../src/crypto/rc4.c \
    ../src/crypto/md5.c \
    ../src/crypto/hmac.c \
    ../src/crypto/bigint.c \
    ../src/crypto/aes.c \
    audiooutput.cpp \
    raopservice.cpp \
    mainapplication.cpp \
    raopcallbackhandler.cpp \
    ../src/logger.c

HEADERS  += \
    audiooutput.h \
    raopservice.h \
    mainapplication.h \
    raopcallbackhandler.h

FORMS    += mainwindow.ui

RESOURCES += \
    AirTV.qrc







































