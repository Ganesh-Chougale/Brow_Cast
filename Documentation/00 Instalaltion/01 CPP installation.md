1. Download & install exe from this 
`https://www.msys2.org/`  
choose windows 64  

2. right after installation the terminal will open
```
pacman -Syu
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-toolchain
pacman -S mingw-w64-x86_64-asio
pacman -S mingw-w64-x86_64-nlohmann-json
pacman -S mingw-w64-x86_64-libjpeg-turbo
pacman -S mingw-w64-x86_64-openssl
```
run these commands  

1. find path like this
MYSYS2\mingw64\bin
& add it to system variable path  
```bash
g++ --version
```  