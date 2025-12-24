#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>
#include <vector>

class Downloader {
public:
    Downloader();
    
    std::vector<std::string> searchMusic(const std::string& query);
    bool downloadTrack(const std::string& url, const std::string& outputPath);
    std::string convertToMP3(const std::string& inputFile);
    
private:
    std::string sanitizeFilename(const std::string& filename);
    std::string executeCommand(const std::string& cmd);
};

#endif
