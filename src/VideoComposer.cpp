#include "VideoComposer.h"
#include <cstdlib>
#include <cmath>
#include <iostream>

double VideoComposer::calculateFPS(int frameCount, double maxDurationSeconds) {
    if (maxDurationSeconds <= 0) {
        return 30.0;
    }
    double fps = frameCount / maxDurationSeconds;
    return std::ceil(fps);
}

bool VideoComposer::executeFFmpegCommand(const std::string& command) {
    int result = std::system(command.c_str());
    return result == 0;
}

bool VideoComposer::composeVideo(const std::string& inputPattern, const std::string& outputFile, int frameCount, double maxDurationSeconds) {
    double fps = calculateFPS(frameCount, maxDurationSeconds);

    std::string command = "ffmpeg -framerate " + std::to_string(fps) +
                         " -i \"" + inputPattern + "\" -c:v libx264 -pix_fmt yuv420p -t " +
                         std::to_string(maxDurationSeconds) + " \"" + outputFile + "\" -y";

    std::cout << "Executing FFmpeg command: " << command << std::endl;

    return executeFFmpegCommand(command);
}
