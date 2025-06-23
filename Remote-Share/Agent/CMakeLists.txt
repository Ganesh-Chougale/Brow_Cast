cmake_minimum_required(VERSION 3.10)
project(RemoteShareAgent CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(BOOST_ROOT "C:/local/boost_1_88_0")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_DEBUG ON)  # Enable debug output - useful for troubleshooting Boost finds
find_package(Boost 1.88.0 REQUIRED COMPONENTS thread system)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost is required but could not be found at ${BOOST_ROOT}")
endif()
message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost Libraries: ${Boost_LIBRARIES}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
ExternalProject_Add(libjpeg-turbo
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/libjpeg-turbo-main
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libjpeg-turbo-install
        -DCMAKE_INSTALL_LIBDIR=lib  # <--- ADDED: Ensures libs go into 'lib'
    BUILD_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/libjpeg-turbo-install/lib/jpeg.lib
        ${CMAKE_BINARY_DIR}/libjpeg-turbo-install/lib/turbojpeg.lib # <--- ADDED: For turbojpeg library
)
include_directories(${CMAKE_BINARY_DIR}/libjpeg-turbo-install/include)
set(AGENT_SRCS
    src/main.cpp
    src/CaptureManager.cpp
    src/ImageProcessor.cpp
    src/WebSocketClient.cpp
    src/WindowEnumerator.cpp
)
add_executable(RemoteShareAgent ${AGENT_SRCS})
target_link_libraries(RemoteShareAgent
    PRIVATE
        ${CMAKE_BINARY_DIR}/libjpeg-turbo-install/lib/jpeg.lib
        ${CMAKE_BINARY_DIR}/libjpeg-turbo-install/lib/turbojpeg.lib
        ${Boost_LIBRARIES}
        Gdiplus.lib           # For GDI+ operations (still used for now, will remove later if fully on libjpeg-turbo)
        ws2_32                # Windows Sockets (networking)
        User32.lib            # User Interface services
        Gdi32.lib             # Graphics Device Interface services
)
target_include_directories(RemoteShareAgent PRIVATE ${Boost_INCLUDE_DIRS})
