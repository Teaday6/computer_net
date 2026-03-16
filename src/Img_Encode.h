#pragma once

#include "Encoder.h"
#include <string>

class Img_Encode {
public:
    static bool encodeFile(const std::string& inputFile, const std::string& outputFile, double maxDurationSeconds = 10.0);
    static bool encodeData(const std::vector<uint8_t>& data, const std::string& outputFile, double maxDurationSeconds = 10.0);

private:
    static Encoder encoder;
};
