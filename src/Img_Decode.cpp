#include "Img_Decode.h"
#include "FrameConfig.h"
#include "CRC16.h"
#include <iostream>
#include <algorithm>
#include <cmath>

bool Img_Decoder::isInsideMarker(int col, int row, int gridCols, int gridRows) const noexcept {
    int mb = FrameConfig::MARKER_BLOCK;
    if (col < mb && row < mb) return true;
    if (col >= gridCols - mb && row < mb) return true;
    if (col < mb && row >= gridRows - mb) return true;
    return false;
}

bool Img_Decoder::alignAndWarpImage(const cv::Mat& inputImage, cv::Mat& outputWarped, bool debugMode) {
    cv::Mat gray;
    cv::cvtColor(inputImage, gray, cv::COLOR_BGR2GRAY);

    auto attemptStrategy = [&](const cv::Mat& binImg) -> std::vector<cv::Point2f> {
        // 【物理外挂开启】：形态学闭运算！利用我们的白色保护圈，
        // 强行把由于屏幕反光、摩尔纹导致的黑块内部裂缝给缝合起来！
        cv::Mat morph;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::morphologyEx(binImg, morph, cv::MORPH_CLOSE, kernel);

        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        // 寻找嵌套轮廓
        cv::findContours(morph, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

        std::vector<cv::Point2f> candidates;
        for (size_t i = 0; i < contours.size(); i++) {
            int childIdx = hierarchy[i][2];
            if (childIdx != -1) {
                int grandChildIdx = hierarchy[childIdx][2];
                // 寻找具有“祖-父-孙”三层结构的标记
                if (grandChildIdx != -1) {
                    double area = cv::contourArea(contours[i]);
                    // 随着 CELL_SIZE=16，靶心面积会变大，范围放宽
                    if (area > 100 && area < 25000) {
                        cv::Moments M = cv::moments(contours[i]);
                        if (M.m00 > 0) {
                            candidates.push_back(cv::Point2f(static_cast<float>(M.m10 / M.m00), static_cast<float>(M.m01 / M.m00)));
                        }
                    }
                }
            }
        }

        if (candidates.size() < 3) return {};

        double maxTriArea = -1;
        std::vector<cv::Point2f> best3;

        for (size_t i = 0; i < candidates.size(); i++) {
            for (size_t j = i + 1; j < candidates.size(); j++) {
                for (size_t k = j + 1; k < candidates.size(); k++) {
                    cv::Point2f p1 = candidates[i];
                    cv::Point2f p2 = candidates[j];
                    cv::Point2f p3 = candidates[k];

                    double area = 0.5 * std::abs(p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y));
                    if (area > maxTriArea) {
                        maxTriArea = area;
                        best3 = { p1, p2, p3 };
                    }
                }
            }
        }
        return best3;
        };

    cv::Mat binImg;
    std::vector<cv::Point2f> best3;

    // 策略1：Otsu
    cv::threshold(gray, binImg, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    best3 = attemptStrategy(binImg);

    // 策略2：高斯自适应 (应对严重反光)
    if (best3.empty()) {
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
        cv::adaptiveThreshold(blurred, binImg, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, 51, 15);
        best3 = attemptStrategy(binImg);
    }

    if (best3.size() != 3) return false;

    // 分配 TL, TR, BL
    cv::Point2f tl(0, 0), tr(0, 0), bl(0, 0);
    float minSum = 1e9;
    for (const auto& pt : best3) {
        if (pt.x + pt.y < minSum) { minSum = pt.x + pt.y; tl = pt; }
    }
    std::vector<cv::Point2f> rem;
    for (const auto& pt : best3) {
        if (pt != tl) rem.push_back(pt);
    }
    if (rem[0].x > rem[1].x) { tr = rem[0]; bl = rem[1]; }
    else { tr = rem[1]; bl = rem[0]; }

    cv::Point2f br = cv::Point2f(tr.x + bl.x - tl.x, tr.y + bl.y - tl.y);
    std::vector<cv::Point2f> srcPoints = { tl, tr, br, bl };

    // 中心偏移量为 4.5 个大网格
    float centerOffset = 4.5f * FrameConfig::CELL_SIZE;
    float maxDim = static_cast<float>(FrameConfig::IMAGE_WIDTH);
    std::vector<cv::Point2f> dstPoints = {
        cv::Point2f(centerOffset, centerOffset),
        cv::Point2f(maxDim - centerOffset, centerOffset),
        cv::Point2f(maxDim - centerOffset, maxDim - centerOffset),
        cv::Point2f(centerOffset, maxDim - centerOffset)
    };

    cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
    cv::warpPerspective(gray, outputWarped, perspectiveMatrix, cv::Size(FrameConfig::IMAGE_WIDTH, FrameConfig::IMAGE_HEIGHT));

    return true;
}

bool Img_Decoder::extractBitsFromGrid(const cv::Mat& warpedGray, std::vector<bool>& allBits, bool debugMode) {
    int cs = FrameConfig::CELL_SIZE;
    int halfCell = cs / 2;
    // 取样框扩大，利用大网格的优势！
    int safeMargin = cs / 4;

    auto sampleBrightness = [&](double cx, double cy) {
        cv::Rect roi(static_cast<int>(cx) - safeMargin, static_cast<int>(cy) - safeMargin, safeMargin * 2 + 1, safeMargin * 2 + 1);
        roi &= cv::Rect(0, 0, warpedGray.cols, warpedGray.rows);
        if (roi.width <= 0 || roi.height <= 0) return 128.0;
        return cv::mean(warpedGray(roi))[0];
        };

    double avgBlack = (sampleBrightness(4.5 * cs, 4.5 * cs) + sampleBrightness(warpedGray.cols - 4.5 * cs, 4.5 * cs) + sampleBrightness(4.5 * cs, warpedGray.rows - 4.5 * cs)) / 3.0;
    double avgWhite = (sampleBrightness(4.5 * cs, 2.5 * cs) + sampleBrightness(warpedGray.cols - 4.5 * cs, 2.5 * cs) + sampleBrightness(4.5 * cs, warpedGray.rows - 2.5 * cs)) / 3.0;

    double dynamicThresh = (avgBlack + avgWhite) / 2.0;
    if (std::abs(avgWhite - avgBlack) < 20) dynamicThresh = 128.0;

    cv::Mat warpedSmooth;
    cv::GaussianBlur(warpedGray, warpedSmooth, cv::Size(3, 3), 0);

    for (int row = 0; row < FrameConfig::GRID_ROWS; row++) {
        for (int col = 0; col < FrameConfig::GRID_COLS; col++) {
            if (isInsideMarker(col, row, FrameConfig::GRID_COLS, FrameConfig::GRID_ROWS)) continue;

            int cx = col * cs + halfCell;
            int cy = row * cs + halfCell;

            cv::Rect roi(cx - safeMargin, cy - safeMargin, safeMargin * 2 + 1, safeMargin * 2 + 1);
            roi &= cv::Rect(0, 0, warpedSmooth.cols, warpedSmooth.rows);
            if (roi.width <= 0 || roi.height <= 0) continue;

            cv::Scalar meanVal = cv::mean(warpedSmooth(roi));
            bool bit = (meanVal[0] < dynamicThresh);
            allBits.push_back(bit);
        }
    }
    return true;
}

bool Img_Decoder::decodeImage(const cv::Mat& inputImage, std::vector<bool>& outDataBits, uint16_t& outFrameNum, bool debugMode) {
    outDataBits.clear();
    outFrameNum = 0;

    cv::Mat warpedGray;
    if (!alignAndWarpImage(inputImage, warpedGray, debugMode)) return false;

    std::vector<bool> allBits;
    if (!extractBitsFromGrid(warpedGray, allBits, debugMode)) return false;

    size_t expectedTotalBits = FrameConfig::BITS_PER_FRAME - FrameConfig::MARKER_CELLS;
    if (allBits.size() < expectedTotalBits) return false;

    uint16_t frameNum = 0;
    for (int i = 0; i < 16; i++) frameNum = (frameNum << 1) | (allBits[i] ? 1 : 0);

    uint16_t parsedCrc = 0;
    size_t crcStartIdx = allBits.size() - 16;
    for (size_t i = crcStartIdx; i < allBits.size(); i++) parsedCrc = (parsedCrc << 1) | (allBits[i] ? 1 : 0);

    std::vector<bool> bitsForCrcCheck(allBits.begin(), allBits.begin() + crcStartIdx);
    if (CRC16::calculate(bitsForCrcCheck) != parsedCrc) return false;

    outDataBits.assign(allBits.begin() + 16, allBits.begin() + crcStartIdx);
    outFrameNum = frameNum;
    return true;
}