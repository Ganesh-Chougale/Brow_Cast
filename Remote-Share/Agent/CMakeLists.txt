cmake_minimum_required(VERSION 3.10)
project(RemoteShareAgent CXX)

# Set Windows version definitions before any includes
add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
add_definitions(-DNTDDI_VERSION=0x0A000000)  # Windows 10
add_definitions(-DWINVER=0x0A00)  # Windows 10
add_definitions(-D_WIN32_IE=0x0A00)  # IE 10.0

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

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Set source files using wildcard
# WARNING: Using file(GLOB) is generally discouraged in CMake for source files
# as it can lead to issues with dependency tracking and new file detection.
# Consider listing files explicitly for robust builds.
file(GLOB AGENT_SRCS "src/*.cpp") 

# Create executable
add_executable(RemoteShareAgent ${AGENT_SRCS})

# Set include directories
target_include_directories(RemoteShareAgent PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/websocketpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    C:/msys64/mingw64/include
    ${OPENSSL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(RemoteShareAgent PRIVATE
    jpeg
    turbojpeg
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
    shcore
    user32
    gdi32
    dwmapi
    ws2_32
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Add compile definitions if needed
target_compile_definitions(RemoteShareAgent PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)