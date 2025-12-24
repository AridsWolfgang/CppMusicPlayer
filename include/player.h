#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <vector>

class Player {
public:
    Player();
    ~Player();
    
    void play(const std::string& filePath);
    void pause();
    void stop();
    void setVolume(int volume); // 0-100
    bool isPlaying() const;
    
    std::vector<std::string> getPlaylist();
    void addToPlaylist(const std::string& filePath);
    void clearPlaylist();
    
private:
    bool playing;
    std::vector<std::string> playlist;
};

#endif
