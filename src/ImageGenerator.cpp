#include "ImageGenerator.h"
#include "FrameConfig.h"
#include <windows.h>
#include <string>

bool ImageGenerator::isInsideMarker(int col, int row, int gridCols, int gridRows) {
    int mb = FrameConfig::MARKER_BLOCK;
    if (col < mb && row < mb) return true;
    if (col >= gridCols - mb && row < mb) return true;
    if (col < mb && row >= gridRows - mb) return true;
    if (col >= gridCols - mb && row >= gridRows - mb) return true; // [新增] 屏蔽右下角
    return false;
}

void ImageGenerator::drawHuiMarker(cv::Mat& image, int startGridCol, int startGridRow, int cellSize) {
    cv::Rect outer(startGridCol * cellSize, startGridRow * cellSize, 7 * cellSize, 7 * cellSize);
    cv::rectangle(image, outer, cv::Scalar(0, 0, 0), -1);

    cv::Rect middle((startGridCol + 1) * cellSize, (startGridRow + 1) * cellSize, 5 * cellSize, 5 * cellSize);
    cv::rectangle(image, middle, cv::Scalar(255, 255, 255), -1);

    cv::Rect inner((startGridCol + 2) * cellSize, (startGridRow + 2) * cellSize, 3 * cellSize, 3 * cellSize);
    cv::rectangle(image, inner, cv::Scalar(0, 0, 0), -1);
}

cv::Mat ImageGenerator::generateFrameImage(uint16_t frameNumber, const std::vector<bool>& dataBits, uint16_t crc) {
    cv::Mat image(FrameConfig::IMAGE_HEIGHT, FrameConfig::IMAGE_WIDTH, CV_8UC3);
    image.setTo(cv::Scalar(255, 255, 255)); // 纯白底色，形成保护圈

    // 绘制 4 个角的定位标记 [修改处]
    drawHuiMarker(image, 1, 1, FrameConfig::CELL_SIZE); // 左上 (TL)
    drawHuiMarker(image, FrameConfig::GRID_COLS - 8, 1, FrameConfig::CELL_SIZE); // 右上 (TR)
    drawHuiMarker(image, 1, FrameConfig::GRID_ROWS - 8, FrameConfig::CELL_SIZE); // 左下 (BL)
    drawHuiMarker(image, FrameConfig::GRID_COLS - 8, FrameConfig::GRID_ROWS - 8, FrameConfig::CELL_SIZE); // 右下 (BR)

    std::vector<bool> payloadBits;
    for (int i = 15; i >= 0; i--) payloadBits.push_back((frameNumber >> i) & 1);
    payloadBits.insert(payloadBits.end(), dataBits.begin(), dataBits.end());
    for (int i = 15; i >= 0; i--) payloadBits.push_back((crc >> i) & 1);

    drawAllBitsToGrid(image, payloadBits);
    return image;
}

void ImageGenerator::drawBitCell(cv::Mat& image, int gridCol, int gridRow, bool bit) {
    int pixelX = gridCol * FrameConfig::CELL_SIZE;
    int pixelY = gridRow * FrameConfig::CELL_SIZE;
    cv::Scalar color = bit ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);
    cv::Rect cell(pixelX, pixelY, FrameConfig::CELL_SIZE, FrameConfig::CELL_SIZE);
    cv::rectangle(image, cell, color, -1);
}

void ImageGenerator::drawAllBitsToGrid(cv::Mat& image, const std::vector<bool>& bits) {
    int bitIndex = 0;
    for (int row = 0; row < FrameConfig::GRID_ROWS && bitIndex < static_cast<int>(bits.size()); row++) {
        for (int col = 0; col < FrameConfig::GRID_COLS && bitIndex < static_cast<int>(bits.size()); col++) {
            if (isInsideMarker(col, row, FrameConfig::GRID_COLS, FrameConfig::GRID_ROWS)) continue;
            drawBitCell(image, col, row, bits[bitIndex]);
            bitIndex++;
        }
    }
}

bool ImageGenerator::saveFrameImage(const cv::Mat& image, const std::string& filepath) {
    return cv::imwrite(filepath, image);
}

bool ImageGenerator::saveAllFrames(const std::vector<cv::Mat>& frames, const std::string& outputDir) {
    std::wstring wOutputDir(outputDir.begin(), outputDir.end());
    CreateDirectoryW(wOutputDir.c_str(), NULL);
    for (size_t i = 0; i < frames.size(); i++) {
        std::string filepath = outputDir + "/frame_" + std::string(4 - std::to_string(i).length(), '0') + std::to_string(i) + ".png";
        if (!saveFrameImage(frames[i], filepath)) return false;
    }
    return true;
}
