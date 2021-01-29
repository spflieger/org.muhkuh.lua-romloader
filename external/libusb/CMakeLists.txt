cmake_minimum_required(VERSION 3.7)

#----------------------------------------------------------------------------
#
# Build libusb and create a CMake component.
#
PROJECT("libusb")

SET(VERSION_EXTERNAL "1.0.23")
SET(VERSION_EXTERNAL_POST "")

INCLUDE(ExternalProject)


#----------------------------------------------------------------------------
#
# Build the project.
#
SET(LIBUSB_CMAKE_ARGS "")
LIST(APPEND LIBUSB_CMAKE_ARGS "-DPRJ_DIR=${PRJ_DIR}")
LIST(APPEND LIBUSB_CMAKE_ARGS "-DWORKING_DIR=${WORKING_DIR}")
LIST(APPEND LIBUSB_CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
LIST(APPEND LIBUSB_CMAKE_ARGS "-DVERSION_EXTERNAL=${VERSION_EXTERNAL}")
LIST(APPEND LIBUSB_CMAKE_ARGS "-DVERSION_EXTERNAL_POST=${VERSION_EXTERNAL_POST}")

ExternalProject_Add(TARGET_libusb
                    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}.tar.bz2
                    URL_HASH SHA1=0140200398306b5e91c294d4a3c0f63aa0ef07b3
                    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/templates ${CMAKE_CURRENT_BINARY_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}/src/TARGET_libusb
                    CMAKE_ARGS ${LIBUSB_CMAKE_ARGS}
                    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install DESTDIR=${CMAKE_CURRENT_BINARY_DIR}
)
