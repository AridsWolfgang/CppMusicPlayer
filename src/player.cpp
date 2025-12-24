#include "player.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

Player::Player() : playing(false) {}

Player::~Player() {
    stop();
}

void Player::play(const std::string& filePath) {
    if (!fs::exists(filePath)) {
        std::cout << "File not found: " << filePath << std::endl;
        return;
    }
    
    // In a real implementation, use a library like:
    // - SDL_mixer
    // - PortAudio
    // - libVLC
    // - SFML Audio
    
    std::cout << "Playing: " << filePath << std::endl;
    playing = true;
    
    // For demonstration, we'll just show a message
    // Actual audio playback requires audio library integration
}

void Player::pause() {
    playing = !playing;
    std::cout << (playing ? "Resumed" : "Paused") << std::endl;
}

void Player::stop() {
    playing = false;
    std::cout << "Stopped playback" << std::endl;
}

void Player::setVolume(int volume) {
    std::cout << "Volume set to: " << volume << "%" << std::endl;
}

bool Player::isPlaying() const {
    return playing;
}

std::vector<std::string> Player::getPlaylist() {
    std::vector<std::string> files;
    
    // Get all mp3 files in current directory
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.path().extension() == ".mp3") {
            files.push_back(entry.path().filename().string());
        }
    }
    
    return files;
}

void Player::addToPlaylist(const std::string& filePath) {
    playlist.push_back(filePath);
}

void Player::clearPlaylist() {
    playlist.clear();
}
