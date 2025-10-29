QT += core gui network widgets printsupport

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
#    cpu_monitor.cpp \
    cpu_server.cpp \
    main.cpp \
    qcustomplot.cpp

HEADERS += \
#    cpu_monitor.h \
    cpu_server.h \
    qcustomplot.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Для статической линковки
LIBS += -lpthread
