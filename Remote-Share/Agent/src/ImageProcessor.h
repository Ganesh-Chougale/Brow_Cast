#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H
#include <vector>
#include <string>
#include <cstdint>
#include <turbojpeg.h> 
class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();
    static std::vector<uint8_t> CompressToJpeg(const std::vector<uint8_t>& pixelData, int width, int height, int quality = 80);
    static std::string EncodeToBase64(const std::vector<uint8_t>& binaryData);
private:
    static tjhandle s_jpegCompressor;
    static std::string base64_encode_impl(const std::vector<uint8_t>& in);
};
#endif