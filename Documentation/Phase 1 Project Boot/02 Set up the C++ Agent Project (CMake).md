`Remote-Share\Agent`  
### 1. Create CMake file  
```bash
touch CMakeLists.txt
```  
`CMakeLists.txt`  
```txt
# agent/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(RemoteShareAgent CXX)

# Set C++ standard (e.g., C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Add executable target
add_executable(RemoteShareAgent main.cpp)

# Optional: If you plan to use specific libraries later (e.g., for image compression, WebSockets)
# find_package(ZLIB REQUIRED) # Example: For zlib if needed
# target_link_libraries(RemoteShareAgent ZLIB::ZLIB)
```  
### 2. create main cpp file  
```bash
touch main.cpp
```  
`main.cpp`  
```Cpp
// Remote-Share/Agent
#include <iostream>

int main() {
    std::cout << "RemoteShare Agent started!" << std::endl;
    // Your capture and input logic will go here
    return 0;
}
```  
### 3. Build System Setup  
```bash
# A
mkdir build
cd build
```  
then for windows  
```bash
cmake -G "MinGW Makefiles" ..
mingw32-make
```  
or for linux/macOS  
```bash
cmake ..
make
```  