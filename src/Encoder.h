#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

class Encoder {
public:
    Encoder();
    ~Encoder();

    bool encodeFile(const std::string& inputFile, const std::string& outputFile, double maxDurationSeconds = 10.0);
    bool encodeData(const std::vector<uint8_t>& data, const std::string& outputFile, double maxDurationSeconds = 10.0);

    void setOutputDir(const std::string& outputDir);
    void setMaxDuration(double duration);

private:
    std::string outputDir;
    double maxDurationSeconds;

    std::vector<cv::Mat> generateAllFrames(const std::vector<uint8_t>& data);
    bool saveFramesToDisk(const std::vector<cv::Mat>& frames);
    bool composeVideoFromFrames(int frameCount);
};
