#include "DataEncoder.h"
#include <fstream>
#include <stdexcept>

std::vector<uint8_t> DataEncoder::readBinaryFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + filepath);
    }

    return buffer;
}

std::vector<bool> DataEncoder::bytesToBits(const std::vector<uint8_t>& bytes) {
    std::vector<bool> bits;
    bits.reserve(bytes.size() * 8);

    for (uint8_t byte : bytes) {
        for (int i = 7; i >= 0; i--) {
            bits.push_back((byte >> i) & 1);
        }
    }

    return bits;
}

std::vector<bool> DataEncoder::encodeFrameNumber(uint16_t frameNumber) {
    std::vector<bool> bits;
    bits.reserve(16);

    for (int i = 15; i >= 0; i--) {
        bits.push_back((frameNumber >> i) & 1);
    }

    return bits;
}

std::vector<bool> DataEncoder::encodeCRC16(uint16_t crc) {
    std::vector<bool> bits;
    bits.reserve(16);

    for (int i = 15; i >= 0; i--) {
        bits.push_back((crc >> i) & 1);
    }

    return bits;
}

int DataEncoder::calculateTotalFrames(size_t dataSize, int bitsPerFrame) {
    size_t totalBits = dataSize * 8;
    int frames = static_cast<int>((totalBits + bitsPerFrame - 1) / bitsPerFrame);
    return frames;
}

std::vector<std::vector<bool>> DataEncoder::splitDataIntoFrames(const std::vector<uint8_t>& data, int bitsPerFrame) {
    std::vector<bool> allBits = bytesToBits(data);
    std::vector<std::vector<bool>> frames;

    size_t bitIndex = 0;
    while (bitIndex < allBits.size()) {
        std::vector<bool> frameBits;
        for (int i = 0; i < bitsPerFrame && bitIndex < allBits.size(); i++) {
            frameBits.push_back(allBits[bitIndex++]);
        }

        while (frameBits.size() < static_cast<size_t>(bitsPerFrame)) {
            frameBits.push_back(false);
        }

        frames.push_back(frameBits);
    }

    return frames;
}
