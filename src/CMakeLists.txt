cmake_minimum_required(VERSION 3.1)
project(main)

add_subdirectory(rest)
add_subdirectory(network)
add_subdirectory(system)
add_subdirectory(utility)
add_subdirectory(hal)
add_subdirectory(rtc)

include_directories(./)

find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

#file(GLOB APP_SOURCES "*.cc")
#add_library(rest ${APP_SOURCES})
#target_link_libraries(rest libpistache.a)

if(UNITTEST_ENABLE)
  add_definitions(-DDEVEL)
endif(UNITTEST_ENABLE)                                        

set(CMAKE_EXE_LINKER_FLAGS "")
add_executable(main main.cc)

if(UNITTEST_ENABLE)
target_link_libraries(main rest system_manager network_manager hal utility ssl crypto)
else()
target_link_libraries(main rest system_manager network_manager hal utility rtc_agent ssl crypto mmal_core mmal_util mmal_vc_client vcos bcm_host pear nice srtp2 paho-mqtt3cs rtp curl)

endif(UNITTEST_ENABLE)

install(TARGETS main
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
)
