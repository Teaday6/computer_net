#pragma once

#include <string>
#include <vector>

class FFmpegWrapper {
public:
    static bool checkFFmpegInstalled();
    static bool encodeVideo(const std::string& inputPattern, const std::string& outputFile, double fps, double duration);
    static bool decodeVideo(const std::string& inputFile, const std::string& outputPattern);
    static std::string getVersion();

private:
    static bool executeCommand(const std::string& command);
};
