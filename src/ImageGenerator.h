#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>
#include <string>

class ImageGenerator {
public:
    static cv::Mat generateFrameImage(uint16_t frameNumber, const std::vector<bool>& dataBits, uint16_t crc);
    static bool saveFrameImage(const cv::Mat& image, const std::string& filepath);
    static bool saveAllFrames(const std::vector<cv::Mat>& frames, const std::string& outputDir);

private:
    static void drawBitCell(cv::Mat& image, int gridCol, int gridRow, bool bit);
    static void drawAllBitsToGrid(cv::Mat& image, const std::vector<bool>& bits);
    static void drawHuiMarker(cv::Mat& image, int startGridCol, int startGridRow, int cellSize);
    static bool isInsideMarker(int col, int row, int gridCols, int gridRows);
};