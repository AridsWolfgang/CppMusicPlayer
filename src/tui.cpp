#include "tui.h"
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

TUI::TUI() {
    // Constructor - setup terminal if needed
}

TUI::~TUI() {
    // Cleanup
}

void TUI::clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void TUI::showMainMenu() {
    clearScreen();
    std::cout << R"(
    ╔══════════════════════════════════════╗
    ║       MUSIC DOWNLOADER & PLAYER      ║
    ╠══════════════════════════════════════╣
    ║  1. Search and Download Music        ║
    ║  2. Play Music                       ║
    ║  3. View Playlist                    ║
    ║  4. Exit                             ║
    ╚══════════════════════════════════════╝
    )" << std::endl;
    std::cout << "\nEnter your choice (1-4): ";
}

void TUI::showSearchResults(const std::vector<std::string>& results) {
    clearScreen();
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║        SEARCH RESULTS                ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "║ " << (i + 1) << ". " << results[i];
        // Add spaces to align
        int spaces = 35 - results[i].length();
        for (int j = 0; j < spaces; j++) std::cout << " ";
        std::cout << "║\n";
    }
    
    std::cout << "╚══════════════════════════════════════╝\n";
    std::cout << "\nSelect a track (1-" << results.size() << ") or 0 to cancel: ";
}

void TUI::showPlayerScreen(const std::string& currentTrack) {
    clearScreen();
    std::cout << R"(
    ╔══════════════════════════════════════╗
    ║         NOW PLAYING                  ║
    ╠══════════════════════════════════════╣
    )" << std::endl;
    
    std::cout << "║ " << currentTrack;
    int spaces = 35 - currentTrack.length();
    for (int j = 0; j < spaces; j++) std::cout << " ";
    std::cout << "║\n";
    
    std::cout << R"(
    ╠══════════════════════════════════════╣
    ║  Controls:                           ║
    ║  [P] Play/Pause  [S] Stop           ║
    ║  [N] Next        [B] Back           ║
    ║  [Q] Return to Menu                 ║
    ╚══════════════════════════════════════╝
    )" << std::endl;
}

int TUI::getMenuChoice() {
    int choice;
    std::cin >> choice;
    std::cin.ignore(); // Clear newline
    return choice;
}

std::string TUI::getSearchQuery() {
    std::string query;
    clearScreen();
    std::cout << "Enter search query: ";
    std::getline(std::cin, query);
    return query;
}
