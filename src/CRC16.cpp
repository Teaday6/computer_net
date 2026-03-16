#include "CRC16.h"

uint16_t CRC16::updateCRC(uint16_t crc, uint8_t data) {
    crc ^= (data << 8);
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ POLYNOMIAL;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

uint16_t CRC16::calculate(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = updateCRC(crc, data[i]);
    }
    return crc;
}

uint16_t CRC16::calculate(const std::vector<uint8_t>& data) {
    return calculate(data.data(), data.size());
}

uint16_t CRC16::calculate(const std::vector<bool>& bits) {
    std::vector<uint8_t> bytes;
    uint8_t currentByte = 0;
    int bitCount = 0;

    for (bool bit : bits) {
        currentByte = (currentByte << 1) | (bit ? 1 : 0);
        bitCount++;

        if (bitCount == 8) {
            bytes.push_back(currentByte);
            currentByte = 0;
            bitCount = 0;
        }
    }

    if (bitCount > 0) {
        currentByte <<= (8 - bitCount);
        bytes.push_back(currentByte);
    }

    return calculate(bytes);
}
