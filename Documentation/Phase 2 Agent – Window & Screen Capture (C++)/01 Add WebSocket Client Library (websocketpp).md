### 1. download zip file from this repo  
`https://github.com/zaphoyd/websocketpp`  
extract zip & paste the websocket folder into  
`Remote-Share\Agent\libs`  
this folder  

### 2. Update CMakeLists.txt: Tell CMake where to find websocketpp headers.  
`Remote-Share\Agent\CMakeLists.txt`  
```txt
# agent/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(RemoteShareAgent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Add websocketpp include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)

# If you use Boost (optional for websocketpp but good practice for async ops)
# find_package(Boost 1.70.0 COMPONENTS system REQUIRED)
# target_link_libraries(RemoteShareAgent Boost::system)

# Note: websocketpp requires C++11 or higher and typically Boost.Asio or standalone Asio.
# For simplicity, we can use websocketpp's standalone Asio config.

add_executable(RemoteShareAgent main.cpp)
```  
### 3. Re-Build System Setup  
```bash
# Remote-Share\Agent\build
cmake -G "MinGW Makefiles" ..
mingw32-make
```  