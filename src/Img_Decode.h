#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>
#include <string>

class Img_Decoder {
public:
    Img_Decoder() noexcept = default;
    ~Img_Decoder() noexcept = default;

    // Decodes a single image frame.
    // Set debugMode=true to save intermediate processing images to disk.
    bool decodeImage(const cv::Mat& inputImage, std::vector<bool>& outDataBits, uint16_t& outFrameNum, bool debugMode = false);

private:
    bool isInsideMarker(int col, int row, int gridCols, int gridRows) const noexcept;

    // Upgraded: Uses Adaptive Thresholding and Gaussian Blur
    bool alignAndWarpImage(const cv::Mat& inputImage, cv::Mat& outputWarped, bool debugMode);

    // Upgraded: Uses Area-based (ROI) sampling instead of single pixel
    bool extractBitsFromGrid(const cv::Mat& warpedImage, std::vector<bool>& allBits, bool debugMode);
};