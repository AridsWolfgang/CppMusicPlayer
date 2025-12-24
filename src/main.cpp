#include "tui.h"
#include "downloader.h"
#include "player.h"
#include <iostream>
#include <thread>
#include <chrono>

class MusicApp {
private:
    TUI tui;
    Downloader downloader;
    Player player;
    
public:
    void run() {
        int choice;
        
        do {
            tui.showMainMenu();
            choice = tui.getMenuChoice();
            
            switch(choice) {
                case 1:
                    searchAndDownload();
                    break;
                case 2:
                    playMusic();
                    break;
                case 3:
                    viewPlaylist();
                    break;
                case 4:
                    std::cout << "Goodbye!" << std::endl;
                    break;
                default:
                    tui.showMessage("Invalid choice. Try again.");
            }
            
            if (choice != 4) {
                std::cout << "\nPress Enter to continue...";
                std::cin.ignore();
                std::cin.get();
            }
            
        } while (choice != 4);
    }
    
private:
    void searchAndDownload() {
        std::string query = tui.getSearchQuery();
        std::vector<std::string> results = downloader.searchMusic(query);
        
        if (results.empty()) {
            tui.showMessage("No results found.");
            return;
        }
        
        tui.showSearchResults(results);
        int selection = tui.getMenuChoice();
        
        if (selection > 0 && selection <= results.size()) {
            std::string selected = results[selection - 1];
            std::string filename = selected + ".mp3";
            
            tui.showMessage("Downloading: " + selected);
            
            // In real app, get actual URL
            std::string dummyUrl = "http://example.com/track";
            if (downloader.downloadTrack(dummyUrl, filename)) {
                player.addToPlaylist(filename);
                tui.showMessage("Download completed: " + filename);
            } else {
                tui.showMessage("Download failed.");
            }
        }
    }
    
    void playMusic() {
        auto playlist = player.getPlaylist();
        
        if (playlist.empty()) {
            tui.showMessage("No music files found. Download some first.");
            return;
        }
        
        // For simplicity, play the first track
        std::string currentTrack = playlist[0];
        tui.showPlayerScreen(currentTrack);
        player.play(currentTrack);
        
        // Simple player control loop
        char control;
        bool inPlayer = true;
        
        while (inPlayer) {
            std::cin >> control;
            std::cin.ignore();
            
            switch(tolower(control)) {
                case 'p':
                    player.pause();
                    break;
                case 's':
                    player.stop();
                    break;
                case 'q':
                    inPlayer = false;
                    player.stop();
                    break;
                default:
                    std::cout << "Invalid command." << std::endl;
            }
        }
    }
    
    void viewPlaylist() {
        auto playlist = player.getPlaylist();
        
        std::cout << "\n=== Your Playlist ===\n";
        if (playlist.empty()) {
            std::cout << "No tracks in playlist.\n";
        } else {
            for (size_t i = 0; i < playlist.size(); i++) {
                std::cout << (i + 1) << ". " << playlist[i] << "\n";
            }
        }
        std::cout << "=====================\n";
    }
};

int main() {
    try {
        MusicApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
