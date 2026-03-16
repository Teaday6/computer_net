#include "Img_Encode.h"

Encoder Img_Encode::encoder;

bool Img_Encode::encodeFile(const std::string& inputFile, const std::string& outputFile, double maxDurationSeconds) {
    return encoder.encodeFile(inputFile, outputFile, maxDurationSeconds);
}

bool Img_Encode::encodeData(const std::vector<uint8_t>& data, const std::string& outputFile, double maxDurationSeconds) {
    return encoder.encodeData(data, outputFile, maxDurationSeconds);
}
