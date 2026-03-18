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
    if (col >= gridCols - mb && row >= gridRows - mb) return true; // [新增] 右下角
    return false;
}

bool Img_Decoder::alignAndWarpImage(const cv::Mat& inputImage, cv::Mat& outputWarped, bool debugMode) {
    cv::Mat gray;
    cv::cvtColor(inputImage, gray, cv::COLOR_BGR2GRAY);

    auto attemptStrategy = [&](const cv::Mat& binImg) -> std::vector<cv::Point2f> {
        cv::Mat morph;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::morphologyEx(binImg, morph, cv::MORPH_CLOSE, kernel);

        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(morph, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

        std::vector<cv::Point2f> candidates;
        for (size_t i = 0; i < contours.size(); i++) {
            int childIdx = hierarchy[i][2];
            if (childIdx != -1) {
                int grandChildIdx = hierarchy[childIdx][2];
                if (grandChildIdx != -1) {
                    double area = cv::contourArea(contours[i]);
                    // 面积阈值稍微放宽，应对手机拍摄的不同距离
                    if (area > 50 && area < 35000) {
                        cv::Moments M = cv::moments(contours[i]);
                        if (M.m00 > 0) {
                            candidates.push_back(cv::Point2f(static_cast<float>(M.m10 / M.m00), static_cast<float>(M.m01 / M.m00)));
                        }
                    }
                }
            }
        }

        if (candidates.size() < 4) return {};

        // [核心优化] 遍历所有组合，寻找构成最大合法四边形的 4 个点
        double maxQuadArea = -1;
        std::vector<cv::Point2f> best4;

        for (size_t i = 0; i < candidates.size(); i++) {
            for (size_t j = i + 1; j < candidates.size(); j++) {
                for (size_t k = j + 1; k < candidates.size(); k++) {
                    for (size_t l = k + 1; l < candidates.size(); l++) {
                        std::vector<cv::Point2f> pts = { candidates[i], candidates[j], candidates[k], candidates[l] };

                        // 计算质心进行排序
                        cv::Point2f center(0, 0);
                        for (const auto& p : pts) center += p;
                        center.x /= 4.0f; center.y /= 4.0f;

                        std::vector<cv::Point2f> sorted(4, cv::Point2f(-1, -1));
                        int valid = 0;
                        for (const auto& p : pts) {
                            if (p.x < center.x && p.y < center.y) { sorted[0] = p; valid++; }      // TL
                            else if (p.x >= center.x && p.y < center.y) { sorted[1] = p; valid++; } // TR
                            else if (p.x >= center.x && p.y >= center.y) { sorted[2] = p; valid++; }// BR
                            else if (p.x < center.x && p.y >= center.y) { sorted[3] = p; valid++; } // BL
                        }

                        if (valid == 4) {
                            // 使用对角线叉乘计算四边形面积
                            double area = 0.5 * std::abs((sorted[2].x - sorted[0].x) * (sorted[3].y - sorted[1].y) -
                                (sorted[3].x - sorted[1].x) * (sorted[2].y - sorted[0].y));
                            if (area > maxQuadArea) {
                                maxQuadArea = area;
                                best4 = sorted;
                            }
                        }
                    }
                }
            }
        }
        return best4;
        };

    cv::Mat binImg;
    std::vector<cv::Point2f> best4;

    // 策略1：Otsu
    cv::threshold(gray, binImg, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    best4 = attemptStrategy(binImg);

    // 策略2：高斯自适应 (应对严重反光，手机拍摄主要靠这个)
    if (best4.empty()) {
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
        cv::adaptiveThreshold(blurred, binImg, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, 51, 15);
        best4 = attemptStrategy(binImg);
    }

    if (best4.size() != 4) return false;

    // best4 已经经过 sorted 处理：TL, TR, BR, BL
    float centerOffset = 4.5f * FrameConfig::CELL_SIZE;
    float maxDim = static_cast<float>(FrameConfig::IMAGE_WIDTH);
    std::vector<cv::Point2f> dstPoints = {
        cv::Point2f(centerOffset, centerOffset),                     // TL
        cv::Point2f(maxDim - centerOffset, centerOffset),            // TR
        cv::Point2f(maxDim - centerOffset, maxDim - centerOffset),   // BR
        cv::Point2f(centerOffset, maxDim - centerOffset)             // BL
    };

    // [核心优化] 使用 4 个真实点进行严格的 3D 透视变换
    cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(best4, dstPoints);
    cv::warpPerspective(gray, outputWarped, perspectiveMatrix, cv::Size(FrameConfig::IMAGE_WIDTH, FrameConfig::IMAGE_HEIGHT));

    return true;
}

bool Img_Decoder::extractBitsFromGrid(const cv::Mat& warpedGray, std::vector<bool>& allBits, bool debugMode) {
    int cs = FrameConfig::CELL_SIZE;
    int halfCell = cs / 2;
    int safeMargin = cs / 4;

    // 辅助函数：在一个小区域内采样平均亮度
    auto sampleBrightness = [&](double cx, double cy) {
        cv::Rect roi(static_cast<int>(cx) - safeMargin, static_cast<int>(cy) - safeMargin, safeMargin * 2 + 1, safeMargin * 2 + 1);
        roi &= cv::Rect(0, 0, warpedGray.cols, warpedGray.rows);
        if (roi.width <= 0 || roi.height <= 0) return 128.0;
        return cv::mean(warpedGray(roi))[0];
        };

    // 核心优化：在 4 个角的定位标志处，分别采样局部的“黑块”和“白环”亮度
    // 左上角 (TL)
    double TL_t = (sampleBrightness(4.5 * cs, 4.5 * cs) + sampleBrightness(4.5 * cs, 2.5 * cs)) / 2.0;
    // 右上角 (TR)
    double TR_t = (sampleBrightness(warpedGray.cols - 4.5 * cs, 4.5 * cs) + sampleBrightness(warpedGray.cols - 4.5 * cs, 2.5 * cs)) / 2.0;
    // 左下角 (BL)
    double BL_t = (sampleBrightness(4.5 * cs, warpedGray.rows - 4.5 * cs) + sampleBrightness(4.5 * cs, warpedGray.rows - 2.5 * cs)) / 2.0;
    // 右下角 (BR)
    double BR_t = (sampleBrightness(warpedGray.cols - 4.5 * cs, warpedGray.rows - 4.5 * cs) + sampleBrightness(warpedGray.cols - 4.5 * cs, warpedGray.rows - 2.5 * cs)) / 2.0;

    // 轻微高斯模糊去噪
    cv::Mat warpedSmooth;
    cv::GaussianBlur(warpedGray, warpedSmooth, cv::Size(3, 3), 0);

    for (int row = 0; row < FrameConfig::GRID_ROWS; row++) {
        for (int col = 0; col < FrameConfig::GRID_COLS; col++) {
            // 跳过 4 个角的定位标记
            if (isInsideMarker(col, row, FrameConfig::GRID_COLS, FrameConfig::GRID_ROWS)) continue;

            int cx = col * cs + halfCell;
            int cy = row * cs + halfCell;

            cv::Rect roi(cx - safeMargin, cy - safeMargin, safeMargin * 2 + 1, safeMargin * 2 + 1);
            roi &= cv::Rect(0, 0, warpedSmooth.cols, warpedSmooth.rows);
            if (roi.width <= 0 || roi.height <= 0) {
                allBits.push_back(false);
                continue;
            }

            cv::Scalar meanVal = cv::mean(warpedSmooth(roi));

            // 核心优化：使用双线性插值，为当前网格计算专属的动态阈值
            // 这能够完美抵消手机拍摄时光照不均匀（如左边亮、右边暗）的问题，对纯数字视频也绝对精准
            double x_ratio = static_cast<double>(cx) / warpedGray.cols;
            double y_ratio = static_cast<double>(cy) / warpedGray.rows;

            double top_thresh = TL_t * (1.0 - x_ratio) + TR_t * x_ratio;
            double bottom_thresh = BL_t * (1.0 - x_ratio) + BR_t * x_ratio;
            double dynamicThresh = top_thresh * (1.0 - y_ratio) + bottom_thresh * y_ratio;

            // 亮度低于动态阈值为黑 (1)，高于为白 (0)
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
