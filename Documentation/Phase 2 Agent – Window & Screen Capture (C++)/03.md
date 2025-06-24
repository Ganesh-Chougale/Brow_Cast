<!-- Depraciated step -->
### 1. download zip file from this repo  
`https://github.com/Microsoft/vcpkg`  
extract zip & paste the websocket folder into  
`Remote-Share\Agent\libs`  
this folder or you can post it anywhere. we only need to install this  

### 2. Install this.  
`\vcpkg`  
```bash
bootstrap-vcpkg.bat
```  
```bash
.\vcpkg install libjpeg-turbo:x64-windows-static
```  
### 3. Re-Build System Setup  
```bash
# Remote-Share\Agent\build
rm -rf *
cmake -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=mingw32-make ..
mingw32-make VERBOSE=1
```  