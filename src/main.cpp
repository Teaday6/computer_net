#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <opencv2/opencv.hpp>

// 引入编码和解码的头文件
#include "Encoder.h"
#include "Img_Decode.h"

// 打印命令行使用说明
void printUsage() {
    std::cout << "====================================================\n";
    std::cout << "   Visible Light Communication (VLC) System\n";
    std::cout << "====================================================\n";
    std::cout << "Usage:\n";
    std::cout << "  Encode: App.exe encode <input_file> <output_video.mp4>\n";
    std::cout << "  Decode: App.exe decode <input_video.mp4> <output_file>\n";
    std::cout << "====================================================\n";
}

// 辅助函数：将布尔数组（比特流）转换回字节数组
std::vector<uint8_t> bitsToBytes(const std::vector<bool>& bits) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (size_t j = 0; j < 8 && (i + j) < bits.size(); j++) {
            byte = (byte << 1) | (bits[i + j] ? 1 : 0);
        }
        bytes.push_back(byte);
    }
    return bytes;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printUsage();
        return -1;
    }

    std::string mode = argv[1];
    std::string inputPath = argv[2];
    std::string outputPath = argv[3];

    // ==========================================
    // 模式 1：编码 (Encode)
    // ==========================================
    if (mode == "encode") {
        std::cout << "--- Starting Encoder Mode ---\n";
        Encoder encoder;

        if (encoder.encodeFile(inputPath, outputPath, 10.0)) {
            std::cout << "[SUCCESS] Encoding finished. Video saved to: " << outputPath << "\n";
        }
        else {
            std::cerr << "[ERROR] Encoding failed!\n";
            return -1;
        }
    }
    // ==========================================
    // 模式 2：解码 (Decode)
    // ==========================================
    else if (mode == "decode") {
        std::cout << "--- Starting Decoder Mode ---\n";
        cv::VideoCapture cap(inputPath);
        if (!cap.isOpened()) {
            std::cerr << "[ERROR] Could not open video file: " << inputPath << "\n";
            return -1;
        }

        Img_Decoder decoder;
        std::map<uint16_t, std::vector<bool>> recoveredData;
        cv::Mat frame;
        int frameCounter = 0;
        int successCount = 0;

        std::cout << "Processing video frames...\n";

        while (cap.read(frame)) {
            frameCounter++;
            std::vector<bool> dataBits;
            uint16_t frameNum;

            if (decoder.decodeImage(frame, dataBits, frameNum)) {
                if (recoveredData.find(frameNum) == recoveredData.end()) {
                    recoveredData[frameNum] = dataBits;
                    successCount++;
                    std::cout << "  -> Recovered Frame Index: " << frameNum
                        << " (Total Unique: " << successCount << ")\n";
                }
            }

            if (frameCounter % 30 == 0) {
                std::cout << "Processed " << frameCounter << " frames...\n";
            }
        }

        if (recoveredData.empty()) {
            std::cerr << "[ERROR] Failed to decode any valid frames from the video.\n";
            return -1;
        }

        // 1. 拼接所有的比特流 (正确定义并遍历)
        std::vector<bool> finalBitstream;
        for (auto const& pair : recoveredData) {
            finalBitstream.insert(finalBitstream.end(), pair.second.begin(), pair.second.end());
        }

        // 2. 将比特流转回字节
        std::vector<uint8_t> allRecoveredBytes = bitsToBytes(finalBitstream);

        // --- 【修改方案A：解析头部4字节长度并截断】 ---
        if (allRecoveredBytes.size() < 4) {
            std::cerr << "[ERROR] Recovered data too short to contain length info.\n";
            return -1;
        }

        // 解析前4个字节得到真正的文件长度
        uint32_t originalSize = 0;
        originalSize |= (static_cast<uint32_t>(allRecoveredBytes[0]) << 24);
        originalSize |= (static_cast<uint32_t>(allRecoveredBytes[1]) << 16);
        originalSize |= (static_cast<uint32_t>(allRecoveredBytes[2]) << 8);
        originalSize |= static_cast<uint32_t>(allRecoveredBytes[3]);

        std::cout << "[INFO] Metadata found. Original file size: " << originalSize << " bytes.\n";

        // 安全校验：如果解出来的数据比预计的长，我们只截取 originalSize
        // 如果解出来的数据比预计的短（可能漏拍了最后一帧），则有多少算多少
        if (allRecoveredBytes.size() < (originalSize + 4)) {
            std::cerr << "[WARNING] Data shorter than expected. Truncating to recovered length.\n";
            originalSize = static_cast<uint32_t>(allRecoveredBytes.size() - 4);
        }

        // 从第 5 个字节开始，截取 originalSize 长度的数据
        std::vector<uint8_t> finalData(allRecoveredBytes.begin() + 4, allRecoveredBytes.begin() + 4 + originalSize);

        // 3. 写入最终文件
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "[ERROR] Could not create output file: " << outputPath << "\n";
            return -1;
        }
        outFile.write(reinterpret_cast<const char*>(finalData.data()), finalData.size());
        outFile.close();

        std::cout << "[SUCCESS] Reconstructed file saved with correct size: " << finalData.size() << " bytes.\n";
    }
    // ==========================================
    // 错误处理
    // ==========================================
    else {
        std::cerr << "[ERROR] Unknown mode: " << mode << "\n";
        printUsage();
        return -1;
    }

    return 0;
}