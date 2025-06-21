### installation  
1. node: `https://nodejs.org/en/download`  
2. ffmpeg: `https://ffmpeg.org/download.html`  

in case of `ffmpeg` download zip file, extract & keep in into permanant safeplace like  
`C:\app\ffmpeg\bin`     

we will add both installed path to system variables  
##### check node version  
```bash
node -v
```  
#### Output:  
```console
v20.18.0
```  
##### check ffmpeg version  
```bash
ffmpeg -version
```  
#### Output:  
```console
ffmpeg version 2025-06-17-git-ee1f79b0fa-essentials_build-www.gyan.dev Copyright (c) 2000-2025 the FFmpeg developers
built with gcc 15.1.0 (Rev4, Built by MSYS2 project)
configuration: --enable-gpl --enable-version3 --enable-static --disable-w32threads --disable-autodetect --enable-fontconfig --enable-iconv --enable-gnutls --enable-libxml2 --enable-gmp --enable-bzlib --enable-lzma --enable-zlib --enable-libsrt --enable-libssh --enable-libzmq --enable-avisynth --enable-sdl2 --enable-libwebp --enable-libx264 --enable-libx265 --enable-libxvid --enable-libaom --enable-libopenjpeg --enable-libvpx --enable-mediafoundation --enable-libass --enable-libfreetype --enable-libfribidi --enable-libharfbuzz --enable-libvidstab --enable-libvmaf --enable-libzimg --enable-amf --enable-cuda-llvm --enable-cuvid --enable-dxva2 --enable-d3d11va --enable-d3d12va --enable-ffnvcodec --enable-libvpl --enable-nvdec --enable-nvenc --enable-vaapi --enable-openal --enable-libgme --enable-libopenmpt --enable-libopencore-amrwb --enable-libmp3lame --enable-libtheora --enable-libvo-amrwbenc --enable-libgsm --enable-libopencore-amrnb --enable-libopus --enable-libspeex --enable-libvorbis --enable-librubberband
libavutil      60.  3.100 / 60.  3.100
libavcodec     62.  3.101 / 62.  3.101
libavformat    62.  1.100 / 62.  1.100
libavdevice    62.  0.100 / 62.  0.100
libavfilter    11.  0.100 / 11.  0.100
libswscale      9.  0.100 /  9.  0.100
libswresample   6.  0.100 /  6.  0.100
```  

### get appnames  
powershell:  
```bash
Get-Process | Where-Object {$_.mainWindowTitle} | Format-Table Id, Name, mainWindowtitle -AutoSize
```  
#### Output:  
```console
PS C:\Users\RaSkull> Get-Process | Where-Object {$_.mainWindowTitle} | Format-Table Id, Name, mainWindowtitle -AutoSize

   Id Name            MainWindowTitle
   -- ----            ---------------
 1976 chrome          WhatsApp - Google Chrome
10876 Code            Actual.md - New folder - Visual Studio Code
12136 fdm             Free Download Manager
10148 Notepad         Untitled - Notepad
 8604 TextInputHost   Windows Input Experience
 3020 WindowsTerminal Windows PowerShell
```  