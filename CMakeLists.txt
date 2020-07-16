cmake_minimum_required(VERSION 3.13)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################

project(fty-common-messagebus-ng
    VERSION 1.0.0
    DESCRIPTION "Experemental implemenation of message bus"
)

##############################################################################################################

etn_target(shared ${PROJECT_NAME}
    PUBLIC
        fty/messagebus/dispatcher.h
        fty/messagebus/exception.h
        fty/messagebus/interface.h
        fty/messagebus/message.h
    SOURCES
        src/malamute.cpp
        src/malamute.h
        src/interface.cpp
    USES
        fty-common-logging-ng
        fty-utils
        malamute
        uuid
)
##############################################################################################################
