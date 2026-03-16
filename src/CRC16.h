#pragma once

#include <vector>
#include <cstdint>

class CRC16 {
public:
    static uint16_t calculate(const uint8_t* data, size_t length);
    static uint16_t calculate(const std::vector<uint8_t>& data);
    static uint16_t calculate(const std::vector<bool>& bits);

private:
    static const uint16_t POLYNOMIAL = 0x8005;
    static uint16_t updateCRC(uint16_t crc, uint8_t data);
};
