cmake_minimum_required(VERSION 3.30)
# Set this before project() initializes the Apple toolchain. The application
# and offline Metal compiler must share one explicit deployment target.
if((CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
    (CMAKE_HOST_APPLE AND NOT CMAKE_SYSTEM_NAME)) AND
   (NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL ""))
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS runtime version" FORCE)
endif()
project(GYO-Engine LANGUAGES C CXX)

include(build/cmake/GyoBuild.cmake)
