#include "ImageProcessor.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

tjhandle ImageProcessor::s_jpegCompressor = nullptr;

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

std::string ImageProcessor::base64_encode_impl(const std::vector<uint8_t>& in) {
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4) {
        out.push_back('=');
    }
    return out;
}

ImageProcessor::ImageProcessor() {
}

ImageProcessor::~ImageProcessor() {
}

void ImageProcessor::InitializeCompressor() {
    if (!s_jpegCompressor) {
        s_jpegCompressor = tjInitCompress();
        if (!s_jpegCompressor) {
            const char* error_str = tjGetErrorStr();
            std::cerr << "Failed to initialize libjpeg-turbo compressor: " << (error_str ? error_str : "Unknown error") << std::endl;
            throw std::runtime_error("Failed to initialize libjpeg-turbo compressor.");
        }
    }
}

void ImageProcessor::ShutdownCompressor() {
    if (s_jpegCompressor) {
        tjDestroy(s_jpegCompressor);
        s_jpegCompressor = nullptr;
    }
}

std::vector<uint8_t> ImageProcessor::CompressToJpeg(const std::vector<uint8_t>& pixelData, int width, int height, int quality) {
    std::vector<uint8_t> jpegData;
    if (!s_jpegCompressor) {
        std::cerr << "libjpeg-turbo compressor not initialized." << std::endl;
        return jpegData;
    }
    if (pixelData.empty() || width <= 0 || height <= 0) {
        std::cerr << "Invalid pixel data or dimensions for JPEG compression." << std::endl;
        return jpegData;
    }
    unsigned char* jpegBuf = NULL; 
    unsigned long jpegSize = 0;    
    int pixelFormat = TJPF_BGR; 
    int result = tjCompress2(s_jpegCompressor,
                             pixelData.data(),      
                             width,                 
                             width * 3,             
                             height,                
                             pixelFormat,           
                             &jpegBuf,              
                             &jpegSize,             
                             TJSAMP_420,            
                             quality,               
                             TJFLAG_FASTDCT         
                            );
    if (result != 0) {
        const char* error_str = tjGetErrorStr();
        std::cerr << "Failed to compress image with libjpeg-turbo: " << (error_str ? error_str : "Unknown error") << std::endl;
        if (jpegBuf) { 
            tjFree(jpegBuf);
        }
        return jpegData;
    }
    jpegData.assign(jpegBuf, jpegBuf + jpegSize);
    tjFree(jpegBuf);
    return jpegData;
}

std::string ImageProcessor::EncodeToBase64(const std::vector<uint8_t>& binaryData) {
    return base64_encode_impl(binaryData);
}