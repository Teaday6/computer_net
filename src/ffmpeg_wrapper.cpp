#include "ffmpeg_wrapper.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

bool FFmpegWrapper::executeCommand(const std::string& command) {
    int result = std::system(command.c_str());
    return result == 0;
}

bool FFmpegWrapper::checkFFmpegInstalled() {
    return executeCommand("ffmpeg -version > nul 2>&1");
}

std::string FFmpegWrapper::getVersion() {
    FILE* pipe = _popen("ffmpeg -version", "r");
    if (!pipe) {
        return "Unknown";
    }

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
        if (result.find("ffmpeg version") != std::string::npos) {
            break;
        }
    }
    _pclose(pipe);

    size_t pos = result.find('\n');
    if (pos != std::string::npos) {
        return result.substr(0, pos);
    }
    return result;
}

bool FFmpegWrapper::encodeVideo(const std::string& inputPattern, const std::string& outputFile, double fps, double duration) {
    std::ostringstream command;
    command << "ffmpeg -framerate " << fps
            << " -i \"" << inputPattern << "\""
            << " -c:v libx264 -pix_fmt yuv420p"
            << " -t " << duration
            << " \"" << outputFile << "\" -y";

    std::cout << "Executing: " << command.str() << std::endl;
    return executeCommand(command.str());
}

bool FFmpegWrapper::decodeVideo(const std::string& inputFile, const std::string& outputPattern) {
    std::ostringstream command;
    command << "ffmpeg -i \"" << inputFile << "\""
            << " \"" << outputPattern << "\" -y";

    std::cout << "Executing: " << command.str() << std::endl;
    return executeCommand(command.str());
}
