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

INCLUDEPATH += ../src/include/ ../src/bindings/qt4/
SOURCES += main.cpp\
    ../src/lib/utils.c \
    ../src/lib/sdp.c \
    ../src/lib/rsapem.c \
    ../src/lib/rsakey.c \
    ../src/lib/raop.c \
    ../src/lib/raop_rtp.c \
    ../src/lib/raop_buffer.c \
    ../src/lib/netutils.c \
    ../src/lib/httpd.c \
    ../src/lib/http_response.c \
    ../src/lib/http_request.c \
    ../src/lib/http_parser.c \
    ../src/lib/dnssd.c \
    ../src/lib/base64.c \
    ../src/lib/alac/alac.c \
    ../src/lib/crypto/sha1.c \
    ../src/lib/crypto/rc4.c \
    ../src/lib/crypto/md5.c \
    ../src/lib/crypto/hmac.c \
    ../src/lib/crypto/bigint.c \
    ../src/lib/crypto/aes.c \
    ../src/lib/logger.c \
    ../src/lib/digest.c \
    audiooutput.cpp \
    mainapplication.cpp \
    audiocallbacks.cpp \
    ../src/bindings/qt4/raopservice.cpp \
    ../src/bindings/qt4/raopcallbackhandler.cpp \
    ../src/bindings/qt4/dnssdservice.cpp

HEADERS  += \
    audiooutput.h \
    mainapplication.h \
    audiocallbacks.h \
    ../src/bindings/qt4/raopservice.h \
    ../src/bindings/qt4/raopcallbacks.h \
    ../src/bindings/qt4/raopcallbackhandler.h \
    ../src/bindings/qt4/dnssdservice.h

FORMS    += mainwindow.ui

RESOURCES += \
    AirTV.qrc












