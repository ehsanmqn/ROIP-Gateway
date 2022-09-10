TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c

INCLUDEPATH += $$PWD/../../UPTIN/linphone-sdk/build-desktop/linphone-sdk/desktop/include/
DEPENDPATH += $$PWD/../../UPTIN/linphone-sdk/build-desktop/linphone-sdk/desktop/include/

unix:!macx: LIBS += -L$$PWD/../../UPTIN/linphone-sdk/build-desktop/linphone-sdk/desktop/lib -lgsm -lspeexdsp -lmbedtls -lbelcard -lbelr -lmbedx509 -lmbedcrypto -lsrtp2 -lbv16 -llinphone -lbellesip -lmediastreamer -lbctoolbox -lortp -lspeex
unix:!macx: LIBS += -lcurl -lpthread
