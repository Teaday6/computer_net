#pragma once

#include <vector>
#include <cstdint>
#include <string>

class DataEncoder {
public:
    static std::vector<std::vector<bool>> splitDataIntoFrames(const std::vector<uint8_t>& data, int bitsPerFrame);
    static std::vector<bool> encodeFrameNumber(uint16_t frameNumber);
    static std::vector<bool> encodeCRC16(uint16_t crc);
    static std::vector<bool> bytesToBits(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> readBinaryFile(const std::string& filepath);
    static int calculateTotalFrames(size_t dataSize, int bitsPerFrame);
};
