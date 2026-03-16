#include "Encoder.h"
#include "FrameConfig.h"
#include "DataEncoder.h"
#include "CRC16.h"
#include "ImageGenerator.h"
#include "VideoComposer.h"
#include <iostream>

Encoder::Encoder() : outputDir("frames"), maxDurationSeconds(10.0) {
}

Encoder::~Encoder() {
}

void Encoder::setOutputDir(const std::string& outputDir) {
    this->outputDir = outputDir;
}

void Encoder::setMaxDuration(double duration) {
    this->maxDurationSeconds = duration;
}

bool Encoder::encodeFile(const std::string& inputFile, const std::string& outputFile, double maxDurationSeconds) {
    try {
        std::vector<uint8_t> data = DataEncoder::readBinaryFile(inputFile);
        return encodeData(data, outputFile, maxDurationSeconds);
    }
    catch (const std::exception& e) {
        std::cerr << "Error encoding file: " << e.what() << std::endl;
        return false;
    }
}

bool Encoder::encodeData(const std::vector<uint8_t>& data, const std::string& outputFile, double maxDurationSeconds) {
    this->maxDurationSeconds = maxDurationSeconds;

    std::cout << "Starting encoding process..." << std::endl;
    std::cout << "Original data size: " << data.size() << " bytes" << std::endl;

    // --- [修改方案A：在数据头部插入4字节的文件长度] ---
    uint32_t fileSize = static_cast<uint32_t>(data.size());
    std::vector<uint8_t> payload;
    payload.reserve(data.size() + 4);

    // 按大端序(Big-Endian)存入4字节长度
    payload.push_back(static_cast<uint8_t>((fileSize >> 24) & 0xFF));
    payload.push_back(static_cast<uint8_t>((fileSize >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((fileSize >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(fileSize & 0xFF));

    // 将原始数据追加到长度信息后面
    payload.insert(payload.end(), data.begin(), data.end());
    // --------------------------------------------------

    // 注意这里传入的是带有长度头的 payload
    std::vector<cv::Mat> frames = generateAllFrames(payload);

    if (frames.empty()) {
        std::cerr << "Failed to generate frames" << std::endl;
        return false;
    }

    std::cout << "Generated " << frames.size() << " frames" << std::endl;

    if (!saveFramesToDisk(frames)) {
        std::cerr << "Failed to save frames to disk" << std::endl;
        return false;
    }

    std::cout << "Frames saved to: " << outputDir << std::endl;

    if (!composeVideoFromFrames(static_cast<int>(frames.size()))) {
        std::cerr << "Failed to compose video" << std::endl;
        return false;
    }

    std::cout << "Video successfully created: " << outputFile << std::endl;
    return true;
}

std::vector<cv::Mat> Encoder::generateAllFrames(const std::vector<uint8_t>& data) {
    std::vector<std::vector<bool>> frameDataList = DataEncoder::splitDataIntoFrames(data, FrameConfig::DATA_PAYLOAD_SIZE);

    std::vector<cv::Mat> frames;
    frames.reserve(frameDataList.size());

    for (size_t i = 0; i < frameDataList.size(); i++) {
        uint16_t frameNumber = static_cast<uint16_t>(i);

        std::vector<bool> frameNumberBits = DataEncoder::encodeFrameNumber(frameNumber);

        std::vector<bool> allBits;
        allBits.insert(allBits.end(), frameNumberBits.begin(), frameNumberBits.end());
        allBits.insert(allBits.end(), frameDataList[i].begin(), frameDataList[i].end());

        uint16_t crc = CRC16::calculate(allBits);

        cv::Mat frame = ImageGenerator::generateFrameImage(frameNumber, frameDataList[i], crc);
        frames.push_back(frame);

        if (i % 100 == 0 && i > 0) {
            std::cout << "Generated frame " << i << " of " << frameDataList.size() << std::endl;
        }
    }

    return frames;
}

bool Encoder::saveFramesToDisk(const std::vector<cv::Mat>& frames) {
    return ImageGenerator::saveAllFrames(frames, outputDir);
}

bool Encoder::composeVideoFromFrames(int frameCount) {
    std::string inputPattern = outputDir + "/frame_%04d.png";
    double duration = maxDurationSeconds;

    return VideoComposer::composeVideo(inputPattern, "output.mp4", frameCount, duration);
}