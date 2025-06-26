cmake_minimum_required(VERSION 3.10)
project(RemoteShareAgent CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set Boost debug output
set(Boost_DEBUG ON)

# Find dependencies
find_package(Boost 1.78.0 REQUIRED COMPONENTS thread system)

# Find libjpeg-turbo from system paths
find_path(JPEG_INCLUDE_DIR jpeglib.h PATHS /mingw64/include)
find_library(JPEG_LIBRARY NAMES jpeg libjpeg PATHS /mingw64/lib)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg libturbojpeg PATHS /mingw64/lib)

if(NOT JPEG_INCLUDE_DIR OR NOT JPEG_LIBRARY OR NOT TURBOJPEG_LIBRARY)
    message(FATAL_ERROR "libjpeg-turbo not found. Please install it with: pacman -S mingw-w64-x86_64-libjpeg-turbo")
else()
    message(STATUS "Found libjpeg include: ${JPEG_INCLUDE_DIR}")
    message(STATUS "Found libjpeg library: ${JPEG_LIBRARY}")
    message(STATUS "Found turbojpeg library: ${TURBOJPEG_LIBRARY}")
    
    add_library(jpeg STATIC IMPORTED)
    set_target_properties(jpeg PROPERTIES
        IMPORTED_LOCATION ${JPEG_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${JPEG_INCLUDE_DIR}
    )
    
    add_library(turbojpeg STATIC IMPORTED)
    set_target_properties(turbojpeg PROPERTIES
        IMPORTED_LOCATION ${TURBOJPEG_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${JPEG_INCLUDE_DIR}
    )
endif()

find_package(nlohmann_json 3.11.2 REQUIRED)

# Set source files
set(AGENT_SRCS
    src/main.cpp
    src/CaptureManager.cpp
    src/ImageProcessor.cpp
    src/WebSocketClient.cpp
    src/WindowEnumerator.cpp
    src/InputInjector.cpp
)

# Create executable
add_executable(RemoteShareAgent ${AGENT_SRCS})

# Set include directories
target_include_directories(RemoteShareAgent PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    C:/msys64/mingw64/include
)

# Link libraries
target_link_libraries(RemoteShareAgent PRIVATE
    jpeg
    turbojpeg
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
    Gdiplus.lib
    ws2_32
    User32.lib
    Gdi32.lib
)

# Add compile definitions if needed
target_compile_definitions(RemoteShareAgent PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)
