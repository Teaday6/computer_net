#pragma once

#include <string>

class VideoComposer {
public:
    static bool composeVideo(const std::string& inputPattern, const std::string& outputFile, int frameCount, double maxDurationSeconds);
    static double calculateFPS(int frameCount, double maxDurationSeconds);

private:
    static bool executeFFmpegCommand(const std::string& command);
};
